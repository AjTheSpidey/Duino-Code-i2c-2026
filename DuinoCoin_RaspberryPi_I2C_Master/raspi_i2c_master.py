import hashlib
import json
import multiprocessing
import os
import socket
import threading
import time
import urllib.request
from pathlib import Path

try:
    from smbus2 import SMBus
except ImportError:
    SMBus = None


POOL_URL = "https://server.duinocoin.com/getPool"


def load_config():
    path = Path(__file__).with_name("config.json")
    if not path.exists():
        path = Path(__file__).with_name("config.example.json")
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def get_pool():
    with urllib.request.urlopen(POOL_URL, timeout=10) as response:
        payload = json.loads(response.read().decode("utf-8"))
    return payload["ip"], int(payload["port"]), payload.get("name", "pool")


def recv_line(sock, timeout=30):
    sock.settimeout(timeout)
    data = bytearray()
    while True:
        chunk = sock.recv(1)
        if not chunk:
            raise ConnectionError("socket closed")
        if chunk == b"\n":
            return data.decode("utf-8", errors="replace")
        data.extend(chunk)


class PoolClient:
    def __init__(self, label):
        self.label = label
        self.sock = None

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None

    def connect(self):
        if self.sock:
            return
        host, port, name = get_pool()
        self.sock = socket.create_connection((host, port), timeout=20)
        version = recv_line(self.sock)
        print(f"[{self.label}] {name} {host}:{port} version={version}")

    def request_job(self, user, job_type, mining_key):
        self.connect()
        parts = ["JOB", user, job_type]
        if mining_key:
            parts.append(mining_key)
        self.sock.sendall((",".join(parts) + "\n").encode("utf-8"))
        return recv_line(self.sock)

    def submit(self, payload):
        self.connect()
        self.sock.sendall((payload + "\n").encode("utf-8"))
        return recv_line(self.sock, timeout=30)


class SlaveState:
    def __init__(self, shared_count=None):
        self.addresses = []
        self.shared_count = shared_count

    def set(self, addresses):
        self.addresses = list(addresses)
        if self.shared_count is not None:
            with self.shared_count.get_lock():
                self.shared_count.value = len(addresses)

    def any(self):
        if self.shared_count is not None:
            return self.shared_count.value > 0
        return bool(self.addresses)

    def count(self):
        if self.shared_count is not None:
            return self.shared_count.value
        return len(self.addresses)


class I2CBus:
    def __init__(self, cfg):
        if SMBus is None:
            raise RuntimeError("Install smbus2 first: python3 -m pip install smbus2")
        self.cfg = cfg
        self.bus = SMBus(cfg["i2c_bus"])
        self.last_scan = 0
        self.slaves = []

    def scan(self, force=False):
        interval = self.cfg["i2c_scan_seconds"] if self.slaves else self.cfg["i2c_empty_scan_seconds"]
        if not force and time.monotonic() - self.last_scan < interval:
            return self.slaves

        self.last_scan = time.monotonic()
        found = []
        for address in range(self.cfg["i2c_first_address"], self.cfg["i2c_last_address"] + 1):
            try:
                self.bus.write_quick(address)
                found.append(address)
            except OSError:
                pass
        if found != self.slaves:
            print(f"[i2c] detected slaves: {found if found else 'none'}")
        self.slaves = found
        return self.slaves

    def write_line(self, address, text):
        for offset in range(0, len(text) + 1, 31):
            chunk = (text + "\n")[offset:offset + 31]
            for char in chunk:
                self.bus.write_byte(address, ord(char))
                time.sleep(self.cfg["i2c_byte_delay_seconds"])

    def read_line(self, address, timeout=5):
        deadline = time.monotonic() + timeout
        data = bytearray()
        while time.monotonic() < deadline:
            try:
                value = self.bus.read_byte(address)
            except OSError:
                return ""
            if value == 10:
                return data.decode("utf-8", errors="replace")
            data.append(value)
        return data.decode("utf-8", errors="replace")


def hash_job(last_hash, expected_hash, difficulty, yield_every):
    expected = expected_hash.lower()
    start = time.perf_counter()
    for nonce in range(difficulty):
        digest = hashlib.sha1((last_hash + str(nonce)).encode("ascii")).hexdigest()
        if digest == expected:
            elapsed = max(time.perf_counter() - start, 0.000001)
            return nonce, nonce / elapsed
        if yield_every and nonce and nonce % yield_every == 0:
            time.sleep(0)
    return 0, 0.0


def local_miner(cfg, slave_state, worker_id=0):
    label = "self" if worker_id == 0 else f"self-{worker_id}"
    client = PoolClient(label)
    while True:
        try:
            job = client.request_job(cfg["user"], cfg["local_job_type"], cfg.get("mining_key", ""))
            last_hash, expected_hash, raw_diff = job.split(",", 2)
            difficulty = int(raw_diff) * 100 + 1
            if cfg.get("master_only", False):
                yield_every = cfg.get("hash_yield_master_only", 0)
            else:
                yield_every = cfg["hash_yield_shared"] if slave_state.any() else cfg["hash_yield_single"]
            nonce, hashrate = hash_job(last_hash, expected_hash, difficulty, yield_every)
            payload = f"{nonce},{hashrate:.2f},Raspberry Pi I2C Master,{cfg['rig']} [M{worker_id}],DUCOIDRPI{worker_id}"
            print(f"[{label}] {payload} -> {client.submit(payload)}")
        except Exception as exc:
            print(f"[{label}] reconnecting after: {exc}")
            client.close()
            time.sleep(3)


def serve_slave_once(cfg, bus, address):
    client = PoolClient(f"i2c-{address}")
    try:
        job = client.request_job(cfg["user"], cfg["slave_job_type"], cfg.get("mining_key", ""))
        bus.write_line(address, job)
        result = bus.read_line(address, timeout=cfg["i2c_slave_timeout_seconds"])
        if not result:
            return

        nonce, micros, *rest = result.split(",")
        elapsed = max(int(micros) * 0.000001, 0.000001)
        hashrate = int(nonce) / elapsed
        ducoid = rest[0] if rest else f"DUCOIDI2C{address:02X}"
        payload = f"{nonce},{hashrate:.2f},AVR I2C,{cfg['rig']} [{address}],{ducoid}"
        print(f"[i2c {address}] {payload} -> {client.submit(payload)}")
    finally:
        client.close()


def i2c_controller(cfg, slave_state):
    if not cfg.get("auto_i2c_slaves", True):
        print("[i2c] disabled")
        return

    bus = I2CBus(cfg)
    while True:
        try:
            slaves = bus.scan()
            slave_state.set(slaves)
            for address in slaves:
                serve_slave_once(cfg, bus, address)
        except Exception as exc:
            print(f"[i2c] recovering after: {exc}")
            time.sleep(2)


def main():
    cfg = load_config()
    shared_slave_count = multiprocessing.Value("i", 0)
    slave_state = SlaveState(shared_slave_count)
    master_only = cfg.get("master_only", False)
    use_all_cores = cfg.get("use_all_cores", True)
    default_workers = os.cpu_count() if use_all_cores else 1
    local_workers = max(1, int(cfg.get("local_mining_processes", default_workers or 1)))

    workers = [
        multiprocessing.Process(target=local_miner, args=(cfg, slave_state, worker_id), daemon=True)
        for worker_id in range(local_workers)
    ]

    threads = [
        threading.Thread(target=i2c_controller, args=(cfg, slave_state), daemon=True),
    ]

    if master_only or not cfg.get("auto_i2c_slaves", True):
        threads = []

    for worker in workers:
        worker.start()
    for thread in threads:
        thread.start()

    while True:
        time.sleep(5)
        print(f"[status] local_workers={local_workers} master_only={master_only} i2c_slaves={slave_state.count()}")


if __name__ == "__main__":
    main()
