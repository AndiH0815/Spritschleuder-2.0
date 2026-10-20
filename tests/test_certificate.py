"""Check actual generated certificates using OpenSSL, without device credentials."""
import pathlib
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from device_certificate import create_certificate

for ip in ("192.168.1.50", "192.168.4.1"):
    with tempfile.TemporaryDirectory() as directory:
        output = pathlib.Path(directory)
        create_certificate(output, ip)
        certificate = str(output / "certificate.pem")
        for flag, name in (("-checkhost", "spritschleuder.local"),
                           ("-checkip", ip), ("-checkip", "192.168.4.1")):
            result = subprocess.run(["openssl", "x509", "-in", certificate, "-noout", flag, name],
                                    check=True, capture_output=True, text=True)
            assert "does match certificate" in result.stdout, result.stdout
        details = subprocess.check_output(["openssl", "x509", "-in", certificate, "-noout", "-ext", "subjectAltName"], text=True)
        assert details.count("DNS:spritschleuder.local") == 1
        assert details.count("IP Address:192.168.4.1") == 1
        certificate_key = subprocess.check_output(["openssl", "x509", "-in", certificate, "-pubkey", "-noout"])
        private_key = subprocess.check_output(["openssl", "pkey", "-in", str(output / "key.pem"), "-pubout"])
        assert certificate_key == private_key
print("HTTPS certificates: spritschleuder.local and both IP access paths, deduplicated SANs and matching private key: PASS")
