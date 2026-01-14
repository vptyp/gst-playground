# gstreamer playground project

It's a project which main purpose is to be a playground with `gstreamer` multimedia framework. Mainly it is focused on video processing and computer vision tasks.

## Technology stack

- gstreamer
- C++20 

## Prerequisites 

Unfortunately, in case of `gstreamer`, `conan` is not really well supported. There are some community recipes, but they are not really up to date and do not cover all the needed functionality.

To address this issue it was decided to move on to the solution, there `gstreamer` would be connected to meson via `pkg_config` and 
if there are some problems with environment - `docker` container could be used with mounted source folder from which execution of the program would happen.

## How to use docker container for development
> note: containers would be configured to use host networking.

To build docker image run the following command from the root folder of the project:
```bash
docker-compose up -d --build
```

After that you will be inside the container and you can use `meson` and `ninja` to build the project:
```bash
docker container exec -it gst-playground_gst-env_1 bash
## Inside the container
conan profile detect
conan install . --build=missing -of docker-build -o *:shared=True -s compiler.cppstd=20
meson setup docker-build --native-file ./docker-build/conan_meson_native.ini
ninja -C docker-build
```

## How to run in docker container

```bash
build$ ./gstPlayground -webrtc ws://localhost:8443
```

After that open  the browser and navigate to `https://localhost:9090` to see the 
JS client application, and you should be able to see Remote Streams instance from the application. If you will press on it, you should be able to see the video of ball moving around.

