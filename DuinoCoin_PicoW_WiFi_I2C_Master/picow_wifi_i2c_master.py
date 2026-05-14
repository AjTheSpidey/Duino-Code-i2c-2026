import gc
import json
import time
import socket
import uhashlib as hashlib
import ubinascii
import network
from machine import I2C, Pin, unique_id


DEFAULTS = {
    "user": "your_username",
    "rig": "PicoW-I2C-2026",
    "mining_key": "",
    "wifi": [{"ssid": "your_wifi_name", "password": "your_wifi_password"}],
    "master_only": False,
    "auto_i2c_slaves": True,
    "master_mines_with_i2c_slaves": True,
    "local_job_type": "ESP32S",
    "slave_job_type": "AVR",
    "slave_miner_name": "Official AVR Miner 4.3",
    "i2c_sda": 4,
    "i2c_scl": 5,
    "i2c_freq": 100000,
    "i2c_first_address": 1,
    "i2c_last_address": 16,
    "i2c_empty_scan_seconds": 15,
    "i2c_active_scan_seconds": 30,
    "i2c_read_timeout_seconds": 5,
    "master_hash_budget_ms": 5,
}


def load_config():
    cfg = DEFAULTS.copy()
    try:
        with open("config.json") as f:
            user_cfg = json.load(f)
        cfg.update(user_cfg)
    except OSError:
        pass
    return cfg


def read_line(sock):
    data = b""
    while True:
        chunk = sock.recv(1)
        if not chunk or chunk == b"\n":
            break
        data += chunk
    return data.decode().strip()


def get_pool():
    addr = socket.getaddrinfo("server.duinocoin.com", 80)[0][-1]
    sock = socket.socket()
    sock.settimeout(10)
    sock.connect(addr)
    sock.send(b"GET /getPool HTTP/1.0\r\nHost: server.duinocoin.com\r\n\r\n")
    data = b""
    while True:
        chunk = sock.recv(256)
        if not chunk:
            break
        data += chunk
    sock.close()
    body = data.split(b"\r\n\r\n", 1)[-1]
    pool = json.loads(body.decode())
    return pool["ip"], int(pool["port"])


class DuinoClient:
    def __init__(self):
        self.sock = None
        self.host = ""
        self.port = 0

    def connect(self):
        if self.sock:
            return
        self.host, self.port = get_pool()
        addr = socket.getaddrinfo(self.host, self.port)[0][-1]
        self.sock = socket.socket()
        self.sock.settimeout(30)
        self.sock.connect(addr)
        print("[pool]", read_line(self.sock))

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
        self.sock = None

    def request_job(self, user, job_type, key):
        self.connect()
        request = "JOB,{},{}".format(user, job_type)
        if key:
            request += "," + key
        self.sock.send((request + "\n").encode())
        return read_line(self.sock)

    def submit(self, payload):
        self.connect()
        self.sock.send((payload + "\n").encode())
        return read_line(self.sock)


class I2CBus:
    def __init__(self, cfg):
        self.cfg = cfg
        self.i2c = I2C(0, sda=Pin(cfg["i2c_sda"]), scl=Pin(cfg["i2c_scl"]), freq=cfg["i2c_freq"])
        self.slaves = []
        self.last_scan = 0

    def scan(self, force=False):
        now = time.time()
        interval = self.cfg["i2c_active_scan_seconds"] if self.slaves else self.cfg["i2c_empty_scan_seconds"]
        if not force and now - self.last_scan < interval:
            return self.slaves
        found = [a for a in self.i2c.scan()
                 if self.cfg["i2c_first_address"] <= a <= self.cfg["i2c_last_address"]]
        if found != self.slaves:
            print("[i2c] slaves", found)
        self.slaves = found
        self.last_scan = now
        return found

    def send_job(self, address, job):
        self.i2c.writeto(address, (job + "\n").encode())

    def read_line(self, address):
        deadline = time.time() + self.cfg["i2c_read_timeout_seconds"]
        data = b""
        while time.time() < deadline:
            try:
                c = self.i2c.readfrom(address, 1)
                if c == b"\n":
                    break
                data += c
            except OSError:
                time.sleep_ms(5)
        return data.decode().strip()


def connect_wifi(cfg):
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.config(pm=0xa11140)
    for cred in cfg.get("wifi", []):
        if not cred.get("ssid"):
            continue
        print("[wifi] connecting", cred["ssid"])
        wlan.connect(cred["ssid"], cred.get("password", ""))
        started = time.time()
        while not wlan.isconnected() and time.time() - started < 15:
            time.sleep_ms(250)
        if wlan.isconnected():
            print("[wifi]", wlan.ifconfig()[0])
            return wlan
    raise RuntimeError("No WiFi profile connected")


def mine_local_once(cfg, client, has_slaves):
    if has_slaves and not cfg["master_mines_with_i2c_slaves"]:
        return
    job = client.request_job(cfg["user"], cfg["local_job_type"], cfg.get("mining_key", ""))
    parts = job.split(",")
    if len(parts) < 3:
        client.close()
        return
    last_hash, expected, raw_diff = parts[0], parts[1], int(parts[2])
    difficulty = raw_diff * 100 + 1
    started = time.ticks_us()
    budget_us = cfg["master_hash_budget_ms"] * 1000
    nonce = 0
    while nonce < difficulty:
        digest = hashlib.sha1((last_hash + str(nonce)).encode()).digest()
        if ubinascii.hexlify(digest).decode() == expected:
            elapsed = time.ticks_diff(time.ticks_us(), started)
            rate = nonce / (elapsed / 1000000)
            ducoid = "DUCOID" + ubinascii.hexlify(unique_id()).decode().upper()
            payload = "{},{:.2f},Pico W I2C Master,{},{}".format(nonce, rate, cfg["rig"], ducoid)
            print("[M]", payload, client.submit(payload))
            return
        nonce += 1
        if has_slaves and time.ticks_diff(time.ticks_us(), started) > budget_us:
            return


def serve_slave_once(cfg, bus, address):
    client = DuinoClient()
    try:
        job = client.request_job(cfg["user"], cfg["slave_job_type"], cfg.get("mining_key", ""))
        bus.send_job(address, job)
        result = bus.read_line(address)
        if not result:
            return
        nonce, elapsed, *rest = result.split(",")
        rate = int(nonce) / (int(elapsed) / 1000000)
        ducoid = rest[0] if rest else "DUCOIDI2C{:02X}".format(address)
        payload = "{},{:.2f},{},{} [{}],{}".format(
            nonce, rate, cfg["slave_miner_name"], cfg["rig"], address, ducoid
        )
        print("[{}]".format(address), payload, client.submit(payload))
    except Exception as exc:
        print("[{}] error {}".format(address, exc))
        client.close()


def main():
    cfg = load_config()
    connect_wifi(cfg)
    bus = I2CBus(cfg)
    master_client = DuinoClient()
    while True:
        slaves = [] if cfg.get("master_only") else bus.scan()
        for address in slaves:
            serve_slave_once(cfg, bus, address)
        mine_local_once(cfg, master_client, bool(slaves))
        gc.collect()


main()
