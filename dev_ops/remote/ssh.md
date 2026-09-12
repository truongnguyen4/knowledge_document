# SSH Remote Connection
**Secure Shell (SSH)** securely connects a client machine to a remote server over a network. It is also the transport used for `git` over SSH and for file transfer with `scp`.

## 1. Key Authentication
![SSH concept diagram](./resources/ssh_concept.png)

SSH authentication uses a matching **public/private key pair**:
- The **public key** is shared with the remote server.
- The **private key** stays securely on the client machine.
- The server verifies the client's identity using the public key, without receiving the private key.

## 2. Generate SSH Keys
Generate a public and private key pair on the client machine.
> Use **ED25519** for most new SSH keys. RSA is also supported when required by an older system.

```bash
ssh-keygen -t ed25519 -C "truong.nguyen4@datalogic.com"
```
| Option | Description |
| --- | --- |
| `-t` | Key type, such as `ed25519` or `rsa`. |
| `-C` | A label for the key, usually an email address. |

The public key is stored in `~/.ssh/id_ed25519.pub`; the private key is stored in `~/.ssh/id_ed25519`.

## 3. Connect to the Server
Add the public key to the remote server's `~/.ssh/authorized_keys` file to enable **passwordless login**.
```bash
ssh user@remote_server
```
| Placeholder | Description |
| --- | --- |
| `user` | Username on the remote server. |
| `remote_server` | Remote server hostname or IP address. |

### Useful Commands

*Verify the connection*
```bash
ssh -T git@remote_server
```

*Copy the public key to the remote server*
```bash
ssh-copy-id user@remote_server
```

*Show detailed connection logs*
```bash
ssh -vvv user@remote_server
```

## 4. File Transfer
*Copy a local file to the server*
```bash
scp /path/to/local/file user@remote_server:/path/to/remote/directory
```

*Copy a remote file to the local machine*
```bash
scp user@remote_server:/path/to/remote/file /path/to/local/directory
```

*Useful `scp` options*
| Option | Description |
| --- | --- |
| `-r` | Copy directories recursively. |
| `-P` | Specify a non-default SSH port. The default is `22`. |
