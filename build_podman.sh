IMAGE_NAME="pspsdk"

if ! podman image exists $IMAGE_NAME
then
	podman pull ghcr.io/pspdev/pspsdk:latest
fi

podman run \
	--rm -it \
	--security-opt label=disable \
	-v ./:/workdir \
	-v ./build_podman.sh:/workdir/build_podman.sh:ro \
	-v ./script:/workdir/script:ro \
	-w /workdir \
	--entrypoint /bin/bash \
	$IMAGE_NAME \
	/workdir/script
