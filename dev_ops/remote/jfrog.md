# JFrog Artifactory CLI
JFrog CLI lets you **upload, download, search, and manage** artifacts in JFrog Artifactory.

## 1. Install JFrog CLI
```bash
curl -fL https://install-cli.jfrog.io | sh
```

## 2. Create an Access Token
In the JFrog web UI, open **Settings**, generate an **access token**, and store it securely for the CLI configuration.

## 3. Configure the Local CLI
Start the interactive configuration:
```bash
jf config add
```

Use the following values when prompted:
| Prompt | Value |
| --- | --- |
| Server identifier | Choose a unique name, such as `datalogic-jfrog`. |
| JFrog URL | `https://jfrog.devops.datalogic.com` |
| Authentication method | `Access token` |
| Access token | Paste the token generated in the JFrog web UI. |
| Reverse proxy | Enter `n`. |

> If JFrog reports **"couldn't extract payload from Access Token"**, enter your JFrog username when prompted.

Select the configured server profile:
```bash
jf config use server-name
```
Replace `server-name` with the identifier selected during configuration.

## 4. Verify the Connection
```bash
jf rt ping
jf config show
jf config use
```

## 5. Common Commands
*Upload an artifact*
```bash
jf rt upload file.zip libs-release-local --flat=false
```

*Download an artifact*
```bash
jf rt download libs-release-local/file.apk ./downloads/
```

*Search a repository*
```bash
jf rt search libs-release-local/
```

*Delete an artifact or directory*
```bash
jf rt delete libs-release-local/android/1.0.0/
```
