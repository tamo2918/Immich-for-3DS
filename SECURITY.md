# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in this project, please report it responsibly by contacting the maintainer via GitHub Private Vulnerability Reporting or opening a confidential issue. Please do not publish security vulnerability details in public issues.

---

## Security Considerations for Users

### 1. Plaintext Storage on Nintendo 3DS SD Card
The Nintendo 3DS hardware does not feature hardware-backed secure key storage (such as a Secure Enclave or TPM). The application configuration file (`sdmc:/3ds/Immich3DS/config.json`) stores your Immich API key in plaintext on the SD card.

**Recommended Practice**:
- Always issue a **scoped API key** in Immich with the minimum permissions necessary for uploading photos (`asset.upload`).
- **Never** use an administrative API key or account with system configuration rights on the 3DS.
- In case of device loss or theft, immediately revoke the API key from the Immich Web UI.

### 2. HTTP vs. HTTPS Connections
- **LAN-Only HTTP**: Connecting over plain `http://` sends API keys and image data unencrypted across the network. This should only ever be used on your private, trusted home Wi-Fi network.
- **Remote / Public Networks**: If accessing Immich from outside your home network, always configure and use HTTPS (`https://`) with a valid TLS certificate.

### 3. Protecting Configuration Files
- Never commit your `config.json` file or share your SD card contents publicly.
- A template file (`config.example.json`) is provided for configuration without real credentials.
