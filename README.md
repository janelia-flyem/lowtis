# Lowtis: Low Latency Image Service [![Picture](https://raw.github.com/janelia-flyem/janelia-flyem.github.com/master/images/HHMI_Janelia_Color_Alternate_180x40.png)](http://www.janelia.org)
##DVID-backed Iamge Service

Lowtis is a library to fetch 2D image data from 3D volumes.  The primary data backend supported
in DVID.  Lowtis aims to optimize the latency of image access in DVID through efficient use of
low-level API and by prefetching and caching data.  It provides a layer of abstaction
to make accessing DVID block and multi-scale data much easier.

## Installation

The primary dependency is [libdvid-cpp](https://github.com/janelia-flyem/libdvid-cpp).  To build an application that uses the library use a C++11 compatible compiler and add '-llowtis' to the compile line.

### Standalone installation

To install lowtis:

    % mkdir build; cd build;
    % cmake -DLIBDVID_INCLUDE=path_to_libdvid_include ..
    % make; make install

Make sure libdvidcpp include and library paths are visible to this build.
These commands will install the library liblowtis.  If you are building this
against libdvid install with conda, add the flag -DCMAKE_PREFIX_PATH=path_to_conda_env.

### Conda installation

The [Miniconda](http://conda.pydata.org/miniconda.html) tool first needs to installed:

```
# Install miniconda to the prefix of your choice, e.g. /my/miniconda

# LINUX:
wget https://repo.continuum.io/miniconda/Miniconda-latest-Linux-x86_64.sh
bash Miniconda-latest-Linux-x86_64.sh

# MAC:
wget https://repo.continuum.io/miniconda/Miniconda-latest-MacOSX-x86_64.sh
bash Miniconda-latest-MacOSX-x86_64.sh

# Activate conda
CONDA_ROOT=`conda info --root`
source ${CONDA_ROOT}/bin/activate root
```
Once conda is in your system path, call the following to install neuroproof:

    % conda create -n CHOOSE_ENV_NAME -c flyem lowtis

Conda allows builder to create multiple environments.  To use the library,
set your library path to PREFIX/CHOOSE_ENV_NAME/lib.

Note: this should work on many distributions of linux and on Mac OSX 10.10+.  For Mac,
you will need to set DYLD_FALLBACK_LIBRARY_PATH to PREFIX/CHOOSE_ENV_NAME/lib.

## Usage

The calling program must initialize configuration options in lowtis/LowtisConfig.h
and interact with the functions in lowtis/lowtis.h.  The simple example below
highlights the main external functionality:

    #include <lowtis/lowtis.h>

    using namespace::lowtis;

    int main()
    {
        // create dvid config
        DVIDLabelblkConfig config;
        
        config.username = "foo@bar.com" 
        config.dvid_server = "127.0.0.1:8000";
        config.dvid_uuid = "abcd";
        config.datatypename = "segmentation";
        
        // create service for 2D image fetching
        ImageService service(config);

        // fetch image from 0,0,0 into buffer
        int width = 800; int height = 600;
        std::vector<int> offset(3,0);
        char* buffer = new char[width*height*8];
        service.retrieve_image(width, height, offset, buffer);
        
        // service.flush_cache(); // to reset cache

        return 0;
    }

## TODO

* Support callback for asynchronously updating caller
* Add unit and integration tests
* Implement prefetching



