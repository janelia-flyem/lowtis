# Lowtis: Low Latency Image Service [![Picture](https://raw.github.com/janelia-flyem/janelia-flyem.github.com/master/images/HHMI_Janelia_Color_Alternate_180x40.png)](http://www.janelia.org)
##DVID-backed Iamge Service

Lowtis is a library to fetch 2D image data from 3D volumes.  The primary data backend supported
in DVID.  Lowtis aims to optimize the latency of image access in DVID through efficient use of
low-level API and by prefetching and caching data.  It provides a layer of abstaction
to make accessing DVID block and multi-scale data much easier.

## Installation

The primary dependency is [libdvid-cpp](https://github.com/janelia-flyem/libdvid-cpp).

### Standalone installation

To install lowtis:

    % mkdir build; cd build;
    % cmake ..
    % make; make install

Make sure libdvidcpp include and library paths are visible to this build.
These commands will install the library liblowtis. 

### Conda installation (TBD)

## Usage

The calling program must initialize configuration options in lowtis/LowtisConfig.h
and interact with the functions in lowtis/lowtis.h.  The simple example below
highlights the main external functionality:

    #include <lowtis/lowtis.h>
    using namespace::lowtis;

    int main()
    {
        // create dvid config
        DVIDConfig config;
        config.bytedepth = 8;
        config.dvid_server = "127.0.0.1:8000";
        config.dvid_uuid = "abcd";
        config.datatypename = "segmentation";

        // create service for 2D image fetching
        ImageService service(config.create_pointer());

        // fetch image from 0,0,0
        int width = 800; int height = 600;
        std::vector<int> offset(3,0);
        libdvid::BinaryDataPtr data = 
            service.retrieve_image(width, height, offset);

        return 0;
    }

## TODO

* Support callback for asynchronously updating caller
* Add unit and integration tests
* Implement prefetching



