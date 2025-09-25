#!/bin/bash
set -e 
set -x

cd ~/code/prawalgangwar/ipxe/src 

# make CONFIG=cloud EMBED=config/cloud/gce.ipxe bin-x86_64-pcbios/ipxe.usb bin-x86_64-efi/ipxe.usb DEBUG=gve,netdevice -j20
make CONFIG=cloud EMBED=config/cloud/gce.ipxe bin-x86_64-pcbios/ipxe.usb bin-x86_64-efi/ipxe.usb DEBUG=gve -j20

cd ~/code/prawalgangwar/ipxe/contrib/cloud

########################################################################################################################
# 1. Clean up any stale temporary buckets from previous runs.
echo "Cleaning up stale temporary buckets..."
for bucket in $(gsutil ls -b gs://ipxe-upload-temp-* 2>/dev/null || true); do
    echo "Deleting old temp bucket: $bucket"
    gsutil -m rm -r -f "$bucket" || echo "Failed to delete $bucket, continuing..."
done

TEMP_BUCKET_URI="gs://ipxe-upload-temp-$$"
TEMP_DIR=$(mktemp -d)

echo "Creating temporary bucket: $TEMP_BUCKET_URI"
gsutil mb "$TEMP_BUCKET_URI"


# 3. Create a tarball named disk.raw.tar.gz containing the image as disk.raw.
echo "Creating tarball..."
cp "../../src/bin-x86_64-efi/ipxe.usb" "$TEMP_DIR/disk.raw"
tar -C "$TEMP_DIR" -czf "$TEMP_DIR/disk.raw.tar.gz" disk.raw

# 4. Upload the tarball to the temporary bucket.
echo "Uploading disk.raw.tar.gz to $TEMP_BUCKET_URI/disk.raw.tar.gz..."
gsutil cp "$TEMP_DIR/disk.raw.tar.gz" "$TEMP_BUCKET_URI/disk.raw.tar.gz"


# 5. Delete existing GCE image since --overwrite was used.
echo "Deleting existing image ipxe-$(date +%Y%m%d)-uefi-x86-64 (if it exists)..."
gcloud compute images delete "ipxe-$(date +%Y%m%d)-uefi-x86-64" \
    --project="prawalg-project" \
    --quiet || true

echo "Creating GCE image ipxe-$(date +%Y%m%d)-uefi-x86-64..."
gcloud compute images create "ipxe-$(date +%Y%m%d)-uefi-x86-64" \
    --project="prawalg-project" \
    --family="ipxe-uefi-x86-64" \
    --source-uri="$TEMP_BUCKET_URI/disk.raw.tar.gz" \
    --guest-os-features="UEFI_COMPATIBLE,GVNIC,IDPF"

########################################################################################################################

date=$(date +%Y%m%d)
rand=$(openssl rand -hex 4)
gcloud compute instances create ipxe-vm-$rand --image-project=prawalg-project --image=ipxe-$date-uefi-x86-64 --metadata-from-file=ipxeboot=gce.ipxe --network-interface nic-type=GVNIC --machine-type=c4-standard-2 --boot-disk-size=10GB

# gcloud compute instances create ipxe-vm-$rand --image-project=prawalg-project --image=ipxe-$date-uefi-x86-64 --metadata-from-file=ipxeboot=gce.ipxe --network-interface nic-type=GVNIC --machine-type=c4-standard-2 --boot-disk-size=10GB
# a4-highgpu-8g

gcloud compute instances get-serial-port-output ipxe-vm-$rand

echo gcloud compute instances get-serial-port-output ipxe-vm-$rand

echo gcloud compute instances delete ipxe-vm-$rand
