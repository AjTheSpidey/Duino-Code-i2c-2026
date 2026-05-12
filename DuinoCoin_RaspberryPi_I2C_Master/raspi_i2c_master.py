import hashlib
import json
import socket
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


def connect_to_pool():
    host, port, name = get_pool()
    sock = socket.create_connection((host, port), timeout=20)
    version = recv_line(sock)
    print(f"[pool] {name} {host}:{port} version={version}")
    return sock


def request_job(sock, user, job_type, mining_key):
    parts = ["JOB", user, job_type]
    if mining_key:
        parts.append(mining_key)
    sock.sendall((",".join(parts) + "\n").encode("utf-8"))
    return recv_line(sock)


def mine_local_once(sock, cfg, has_slaves):
    job = request_job(sock, cfg["user"], cfg["local_job_type"], cfg.get("mining_key", ""))
    last_hash, expected_hash, raw_diff = job.split(",", 2)
    difficulty = int(raw_diff) * 100 + 1
    batch = cfg["hash_batch_shared"] if has_slaves else cfg["hash_batch_single"]

    start = time.perf_counter()
    for nonce in range(difficulty):
        digest = hashlib.sha1((last_hash + str(nonce)).encode("ascii")).hexdigest()
        if digest == expected_hash.lower():
            elapsed = max(time.perf_counter() - start, 0.000001)
            hashrate = nonce / elapsed
            payload = f"{nonce},{hashrate:.2f},Raspberry Pi I2C Master,{cfg['rig']},DUCOIDRPI\n"
            sock.sendall(payload.encode("utf-8"))
            print(f"[self] {payload.strip()} -> {recv_line(sock, timeout=30)}")
            return
        if nonce and nonce % batch == 0:
            time.sleep(0)


class I2CMaster:
    def __init__(self, cfg):
        if SMBus is None:
            raise RuntimeError("Install smbus2 first: python3 -m pip install smbus2")
        self.cfg = cfg
        self.bus = SMBus(cfg["i2c_bus"])
        self.slaves = []
        self.last_scan = 0

    def scan(self, force=False):
        if not force and time.monotonic() - self.last_scan < self.cfg["i2c_scan_seconds"]:
            return self.slaves
        self.last_scan = time.monotonic()
        found = []
        for address in range(self.cfg["i2c_first_address"], self.cfg["i2c_last_address"] + 1):
            try:
                self.bus.write_quick(address)
                found.append(address)
            except OSError:
                pass
        self.slaves = found
        return found

    def write_line(self, address, text):
        for char in text + "\n":
            self.bus.write_byte(address, ord(char))
            time.sleep(0.001)

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


def serve_slave_once(sock, i2c, address, cfg):
    job = request_job(sock, cfg["user"], "AVR", cfg.get("mining_key", ""))
    i2c.write_line(address, job)
    result = i2c.read_line(address)
    if not result:
        return
    nonce, micros, *rest = result.split(",")
    elapsed = max(int(micros) * 0.000001, 0.000001)
    hashrate = int(nonce) / elapsed
    ducoid = rest[0] if rest else f"DUCOIDI2C{address:02X}"
    payload = f"{nonce},{hashrate:.2f},AVR I2C,{cfg['rig']} [{address}],{ducoid}\n"
    sock.sendall(payload.encode("utf-8"))
    print(f"[i2c {address}] {payload.strip()} -> {recv_line(sock, timeout=30)}")


def main():
    cfg = load_config()
    mode = cfg.get("mode", "both").lower()
    i2c = I2CMaster(cfg) if mode in ("i2c", "both") else None
    sock = connect_to_pool()

    while True:
        slaves = i2c.scan() if i2c else []
        if mode in ("i2c", "both"):
            for address in slaves:
                serve_slave_once(sock, i2c, address, cfg)
        if mode in ("single", "both"):
            mine_local_once(sock, cfg, bool(slaves))


if __name__ == "__main__":
    main()
