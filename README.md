# DX12Engine
![Image of Engine](https://shubhamsachdevagames.com/img/portfolio/DX12.png)

For this project I am working on a DirectX 12 rendering engine using D3D12 and Windows32 API, along with the HLSL shader language.

I am acting as the sole programmer of the project. I have implemented real-time graphics features such as a lighting system, physically based rendering (thumbnail), area lights using Linearly transformed cosines (bottom right image), particle system, and subsurface scattering using using spherical gaussians (bottom left image). The engine can also render thousands of lights in real time using a tile based forward renderer (top right image).

On the API side, I have created wrappers for many of DX12's low level implementations, such as wrappers for descriptor heaps, resources, and buffers, which make it more managable to work with DX12's low level of abstraction. I have also implemented a ring buffer descriptor heap, that holds all the resources needed to render the current frame, which allows for easy CPU-GPU synchronization.

I am currently working on integrating the DirectX Raytracing API, which takes advantage of modern GPU architecures' hardware accelerated raytracing capabilities, My raytracer currently supports the GGX material model, and is also capable of both direct and indirect lighting (top left image). I am currently working on adding more raytraced features like, multi-bounce global illumination and refractions, as well adding spatio temoral filter do denoise my results, with a goal to create a hybrid raster/raytracing rendering engine for real-time graphics. I am also working on working on porting features from my DX11 engine such as, ocean simulation, and post processing effects.
