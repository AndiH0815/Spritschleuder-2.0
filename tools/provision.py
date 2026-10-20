#!/usr/bin/env python3
"""Create a PRIVATE initial NVS image; never overwrites an installed NVS backup."""
import argparse,csv,getpass,hashlib,ipaddress,os,pathlib,secrets,subprocess,tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--ip',required=True,help='Reserved local IP for certificate SAN')
p.add_argument('--output',default='private-setup',help='Private output directory (gitignored)')
a=p.parse_args();ip=ipaddress.ip_address(a.ip)
os.umask(0o077)
out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
if any(out.iterdir()):p.error('Output directory must be empty; existing setup will not be overwritten')
ssid=input('Heim-WLAN Name: ');wifi=getpass.getpass('WLAN Passwort: ')
access=getpass.getpass('Bedienpasswort (mindestens 12 Zeichen): ');admin=getpass.getpass('Verwaltungspasswort (mindestens 12 Zeichen): ')
if not 1<=len(ssid.encode())<=32 or len(wifi.encode())>64 or not 12<=len(access.encode())<=128 or not 12<=len(admin.encode())<=128 or access==admin:p.error('Ungueltige oder gleiche Passwoerter')
salt=secrets.token_hex(16)
sans=','.join(dict.fromkeys([f'IP:{ip}','IP:192.168.4.1']))
subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','825','-keyout',str(out/'key.pem'),'-out',str(out/'certificate.pem'),'-subj','/CN=Spritschleuder','-addext',f'subjectAltName={sans}'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
rows=[['key','type','encoding','value'],['wifi','namespace','',''],['ssid','data','string',ssid],['password','data','string',wifi],['web_access','namespace','',''],['salt','data','string',salt]]
for key,value in [('access_hash',access),('admin_hash',admin)]:rows.append([key,'data','string',hashlib.pbkdf2_hmac('sha256',value.encode(),salt.encode(),100000).hex()])
rows.extend([['certificate','file','string',str(out/'certificate.pem')],['private_key','file','string',str(out/'key.pem')]])
with tempfile.TemporaryDirectory(dir=out) as temp:
    csvpath=pathlib.Path(temp)/'setup.csv'
    with csvpath.open('w',newline='') as f:csv.writer(f).writerows(rows)
    idf=pathlib.Path(os.environ['IDF_PATH'])
    subprocess.run(['python',str(idf/'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py'),'generate',str(csvpath),str(out/'initial-nvs.bin'),'0x6000'],check=True)
print('Erstellt. Nur zur ERSTEINRICHTUNG nach Sicherung flashen; nie bestehende Rezepte damit ueberschreiben.')
print('Zertifikat auf dem Tablet als vertrauenswuerdig einrichten. Kein Zugangspasswort liegt im Repository.')
