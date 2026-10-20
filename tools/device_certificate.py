"""Generate the device certificate used by the private initial provisioning tool."""
import ipaddress
import pathlib
import subprocess

def create_certificate(output: pathlib.Path, address: str) -> None:
    address = ipaddress.ip_address(address)
    names = ["DNS:spritschleuder.local", f"IP:{address}", "IP:192.168.4.1"]
    sans = ",".join(dict.fromkeys(names))
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "825",
        "-keyout", str(output / "key.pem"), "-out", str(output / "certificate.pem"),
        "-subj", "/CN=Spritschleuder", "-addext", f"subjectAltName={sans}",
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
