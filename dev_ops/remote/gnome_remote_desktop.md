# GNOME Remote Desktop
GNOME Remote Desktop exposes an **RDP server** on Ubuntu, so the desktop session can be reached from a Windows client with **Remote Desktop Connection**.

## 1. Install GNOME Remote Desktop
```bash
sudo apt update
sudo apt install gnome-remote-desktop -y
```

Enable the RDP backend for the current user:
```bash
grdctl rdp enable
```

## 2. Generate a TLS Certificate
RDP requires a TLS certificate and key. Create a self-signed pair for the host:
```bash
mkdir -p ~/.local/share/gnome-remote-desktop
openssl req -x509 -newkey rsa:4096 \
  -keyout ~/.local/share/gnome-remote-desktop/rdp-tls.key \
  -out ~/.local/share/gnome-remote-desktop/rdp-tls.crt \
  -sha256 -days 3650 -nodes \
  -subj "/CN=$(hostname)"
```
| Option | Description |
| --- | --- |
| `-x509` | Produce a self-signed certificate instead of a signing request. |
| `-newkey rsa:4096` | Generate a new 4096-bit RSA key. |
| `-days` | Certificate lifetime in days. |
| `-nodes` | Store the private key without a passphrase. |
| `-subj` | Certificate subject, here the machine hostname. |

Register the certificate and key with the RDP server:
```bash
grdctl rdp set-tls-cert ~/.local/share/gnome-remote-desktop/rdp-tls.crt
grdctl rdp set-tls-key ~/.local/share/gnome-remote-desktop/rdp-tls.key
```

## 3. Set the Login Credentials
The RDP credentials are separate from the Ubuntu account password.
```bash
grdctl rdp set-credentials USERNAME PASSWORD
```
| Placeholder | Description |
| --- | --- |
| `USERNAME` | Username entered in the RDP client. |
| `PASSWORD` | Password entered in the RDP client. |

Allow the client to control the desktop instead of only viewing it:
```bash
grdctl rdp disable-view-only
```

## 4. Start the Service
```bash
systemctl --user restart gnome-remote-desktop
systemctl --user enable gnome-remote-desktop
```
> `--user` starts the service for the logged-in user. The desktop session must be active for the RDP server to accept connections.

## 5. Verify and Open the Port
*Show the current RDP configuration*
```bash
grdctl status
systemctl --user status gnome-remote-desktop.service
```

*Confirm the server is listening on the RDP port*
```bash
ss -ltnp | grep 3389
```

*Allow RDP traffic through the firewall*
```bash
sudo ufw allow 3389/tcp
```

## 6. Connect from Windows
Open **Remote Desktop Connection** (`mstsc`) and enter the Ubuntu machine's hostname or IP address.
| Field | Value |
| --- | --- |
| Computer | Ubuntu hostname or IP address, such as `192.168.1.10`. |
| Domain | Leave empty unless the machine is domain-joined. |
| Username | Username set with `grdctl rdp set-credentials`. |
| Password | Password set with `grdctl rdp set-credentials`. |

> The self-signed certificate triggers a warning on the first connection. Accept it to continue.
