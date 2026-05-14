import json
import socket
import time
from pathlib import Path

import serial


DEFAULTS = {
    "user": "your_username",
    "rig": "PC-I2C-Bridge-2026",
    "mining_key": "",
    "serial_port": "COM3",
    "serial_baud": 115200,
    "slave_job_type": "AVR",
    "slave_miner_name": "Official AVR Miner 4.3",
    "first_i2c_address": 1,
    "last_i2c_address": 16,
    "scan_empty_seconds": 15,
    "scan_active_seconds": 30,
    "i2c_timeout_ms": 5000,
}


def load_config():
    cfg = DEFAULTS.copy()
    path = Path("config.json")
    if path.exists():
        cfg.update(json.loads(path.read_text()))
    return cfg


def socket_readline(sock):
    data = b""
    while True:
        chunk = sock.recv(1)
        if not chunk or chunk == b"\n":
            break
        data += chunk
    return data.decode(errors="replace").strip()


def get_pool():
    with socket.create_connection(("server.duinocoin.com", 80), timeout=10) as sock:
        sock.sendall(b"GET /getPool HTTP/1.0\r\nHost: server.duinocoin.com\r\n\r\n")
        data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk:
                break
            data += chunk
    body = data.split(b"\r\n\r\n", 1)[-1]
    pool = json.loads(body.decode())
    return pool["ip"], int(pool["port"])


class DuinoClient:
    def __init__(self):
        self.sock = None

    def connect(self):
        if self.sock:
            return
        host, port = get_pool()
        self.sock = socket.create_connection((host, port), timeout=30)
        print("[pool]", socket_readline(self.sock))

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None

    def request_job(self, user, job_type, mining_key=""):
        self.connect()
        request = f"JOB,{user},{job_type}"
        if mining_key:
            request += f",{mining_key}"
        self.sock.sendall((request + "\n").encode())
        return socket_readline(self.sock)

    def submit(self, payload):
        self.connect()
        self.sock.sendall((payload + "\n").encode())
        return socket_readline(self.sock)


class Bridge:
    def __init__(self, cfg):
        self.serial = serial.Serial(cfg["serial_port"], cfg["serial_baud"], timeout=10)
        time.sleep(2)
        while self.serial.in_waiting:
            print("[bridge]", self.serial.readline().decode(errors="replace").strip())

    def command(self, text):
        self.serial.write((text + "\n").encode())
        self.serial.flush()
        return self.serial.readline().decode(errors="replace").strip()

    def scan(self):
        response = self.command("SCAN")
        if not response.startswith("ADDRS"):
            print("[bridge]", response)
            return []
        parts = response.split(",")[1:]
        return [int(p) for p in parts if p]

    def send_job(self, address, job):
        parts = job.split(",")
        if len(parts) < 3:
            return False
        response = self.command(f"JOB,{address},{parts[0]},{parts[1]},{parts[2]}")
        return response == "OK"

    def read_result(self, address, timeout_ms):
        response = self.command(f"READ,{address},{timeout_ms}")
        if response.startswith("DATA,"):
            return response[5:]
        if response not in ("", "TIMEOUT"):
            print("[bridge]", response)
        return ""


def serve_slave(cfg, bridge, address):
    client = DuinoClient()
    try:
        job = client.request_job(cfg["user"], cfg["slave_job_type"], cfg.get("mining_key", ""))
        if not bridge.send_job(address, job):
            return
        result = bridge.read_result(address, cfg["i2c_timeout_ms"])
        if not result:
            return
        nonce, elapsed, *rest = result.split(",")
        hashrate = int(nonce) / (int(elapsed) / 1_000_000)
        ducoid = rest[0] if rest else f"DUCOID-BRIDGE-{address:02X}"
        payload = (
            f"{nonce},{hashrate:.2f},{cfg['slave_miner_name']},"
            f"{cfg['rig']} [{address}],{ducoid}"
        )
        feedback = client.submit(payload)
        print(f"[{address}] {feedback} {hashrate:.2f} H/s")
    except Exception as exc:
        print(f"[{address}] {exc}")
        client.close()


def main():
    cfg = load_config()
    bridge = Bridge(cfg)
    slaves = []
    last_scan = 0
    while True:
        interval = cfg["scan_active_seconds"] if slaves else cfg["scan_empty_seconds"]
        if time.time() - last_scan > interval:
            slaves = [
                a for a in bridge.scan()
                if cfg["first_i2c_address"] <= a <= cfg["last_i2c_address"]
            ]
            print("[scan]", slaves if slaves else "none")
            last_scan = time.time()
        for address in slaves:
            serve_slave(cfg, bridge, address)


if __name__ == "__main__":
    main()
