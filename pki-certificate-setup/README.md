# Full Setup from Scratch

## Step 0: Clone the Golioth Firmware SDK

```bash
git clone https://github.com/golioth/golioth-firmware-sdk.git
cd golioth-firmware-sdk
git submodule update --init --recursive
cd ..
```

## Step 1: Generate Project Root CA (One Time Per Project)

```bashss
git clone https://github.com/gigwegbe/ei-golioth-model-updater-linux.git
cd ei-golioth-model-updater-linux
mkdir -p certs
cd certs
../../golioth-firmware-sdk/scripts/certificates/generate_root_certificate.sh
```

Creates:

- `golioth.crt.pem` — upload this to Golioth
- `golioth.key.pem` — **keep this safe**

## Step 2: Upload CA to Golioth Console

1. Go to **console.golioth.io**
2. Sidebar → **Project Settings** → **Certificates** tab
3. Click **Add CA Certificate**, type = **Root**, upload `golioth.crt.pem`

## Step 3: Generate Device Certificate

```bash
# Still inside certs/
../../golioth-firmware-sdk/scripts/certificates/generate_device_certificate.sh <project_id> <certificate_id> pem
```

Example:

```bash
../../golioth-firmware-sdk/scripts/certificates/generate_device_certificate.sh ei-model-deployment rubik-pi pem
```

## Step 4: Copy to Standard Names

```bash
cp ei-model-deployment-rubik-pi.crt.pem client.crt.pem
cp ei-model-deployment-rubik-pi.key.pem client.key.pem
cd ..
```

## Step 5: Get the Golioth Server Root CA

```bash
cp ../golioth-firmware-sdk/src/isrgrootx1_goliothrootx1.pem .
```
