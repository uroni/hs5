---
# the default layout is 'page'
icon: fas fa-rocket
order: 4
title: Use cases
---

## S3 storage on Hetzner storage box

Using HS5 you can build yourself some cost optimized S3 storage based on Hetzner storage boxes that is cheaper than Hetzner's own S3 offer and many other offers (at the cost of perhaps reduced durability of a single ZFS RAIDz2 system).

 * CAX11 VM at 3,29 €/mo
 * Storage box BX41 20TB at €40.60 max/mo

So 2,1945€ TB/mo, so 0,0021945 € GB/mo. Compared to 99.80€ for 20TB per month at Hetzner S3 - 0,00499 € GB/mo.

(all prices without taxes as of 2026-03)

Setup:

 1. Start CAX11 VM -- you can save 0.5€ per month if you don't need it accessible via ipv4. I like to use Debian, so let's assume that for now
 2. Download HS5 to e.g. `/usr/local/bin/hs5` via `wget https://github.com/uroni/hs5/releases/latest/download/hs5-arm64.xz -O - | xz -d > /usr/local/bin/hs5 && chmod +x /usr/local/bin/hs5`
 3. Create a script that mounts the storage box via CIFS and starts HS5 at e.g. `/root/hs5_start.sh` (replace userid, password and mount path with what you setup in Hetzner):

 ```bash
#!/bin/bash
set -ex

STORAGE_BOX_MOUNTPOINT=/mnt/storagebox
DATA_PATH=$STORAGE_BOX_MOUNTPOINT/hs5_data
WAL_PATH=/var/lib/hs5

mkdir -p "$WAL_PATH"
mkdir -p "$STORAGE_BOX_MOUNTPOINT"

umount -l "$STORAGE_BOX_MOUNTPOINT" || true

sleep 1

while ! test -e "$DATA_PATH"
do
        mount -t cifs //u1111.your-storagebox.de/hs5 "$STORAGE_BOX_MOUNTPOINT" -o nobrl,user=u1111,password=REPLACEME || true
        sleep 1
done

exec hs5 run --init-root-password "REPLACEME" --ip 127.0.0.1 -m "$DATA_PATH" -d "$DATA_PATH" -p 8090 -u "https://hs5.example.com" --stop-on-error --wal-mode full-all-data --loglevel warn --wal-path "$WAL_PATH"
```

Why the script? If Hetzner reboots the server hosting the storage, HS5 will automatically stop on error (`--stop-on-error`). That causes the service to fail and run the script anew. The script unmounts the storage box, then remounts it, making sure it is mounted properly.

4. Install cifs-utils (`apt install cifs-utils`). Mount the storage box manually once and create the hs5 sub-dir `hs5_data`. The script doesn't do this automatically because it checks for the existence of this folder to verify that the storage box was successfully mounted before starting HS5. Afterwards run the script manually to test if it works.
5. Install e.g. apache2 with `apt install apache2`. Change the server name in apache2 to `hs5.example.com`.
6. Install certbot with `apt install certbot python3-certbot-apache` and run certbot to setup SSL for the site.
7. Proxy requests to HS5 from apache by adding 
```
ProxyPreserveHost On
ProxyPass / http://127.0.0.1:8090/
ProxyPassReverse / http://127.0.0.1:8090/
```
to the apache2 site config. Enable proxy module with `a2enmod proxy && a2enmod proxy_http`.
8. Create a systemd service starting the script e.g. at `/etc/systemd/system/hs5.service`:
```
[Unit]
Description=Start HS5

[Service]
ExecStart=/root/hs5_start.sh
Restart=always
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
```
9. Enable systemd service and start it `systemctl daemon-reload && systemctl start hs5`