# Collidoscope

### Install cmake 
```
sudo apt-get install cmake 
```

### Install Required Libraries 

```
sudo apt-get install libxcursor-dev \
libgles2-mesa-dev \
zlib1g-dev \
libfontconfig1-dev \
libmpg123-dev \
libsndfile1 \
libsndfile1-dev \
libpulse-dev \
libasound2-dev \
libcurl4-gnutls-dev \
libgstreamer1.0-dev \
libgstreamer-plugins-bad1.0-dev \
libgstreamer-plugins-base1.0-dev \
gstreamer1.0-libav \
gstreamer1.0-alsa \
gstreamer1.0-pulseaudio \
gstreamer1.0-plugins-bad \
libboost-all-dev \
libxrandr-dev

```

### Clone Repository and Build

Clone repo

`git clone --recursive https://github.com/collidoscopeio/Collidoscope.git`



Apply patch for Collidoscope to Cinder library
```
cd Collidoscope/Cinder
git apply ../JackDevice/Cinder.patch
```

Build Cinder library

```
mkdir build && cd build
cmake .. -DCINDER_TARGET_GL=es2-rpi -DCMAKE_BUILD_TYPE=Release 
make 
```

Build CollidoscopeApp
```
cd ../../CollidoscopeApp
mkdir build && cd build
cmake .. -DCINDER_TARGET_GL=es2-rpi -DCMAKE_BUILD_TYPE=Release 
make 
```

If the compiler complains about libEGL you might need to run rpi-update.
More info on this [here](https://discourse.libcinder.org/t/unable-to-build-apps-on-latest-raspbian/840)

Binary is in `Collidoscope/CollidoscopeApp/build/Release/CollidoscopeApp/`




