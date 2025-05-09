# vsp

vsp is a lightweight OpenGL audio visualizer. A port of [rvsp](https://github.com/cynthia2006/rvsp), the functionality is an exact replica of the original, save for a few things. 

## Installation

### Prerequisites

- Meson (build system)
- GLFW 3.0
- PipeWire

```
$ git clone https://github.com/cynthia2006/vsp
$ cd vsp
$ mkdir build-tmp
$ git clone https://github.com/mborgerding/kissfft
$ make PREFIX=../build-tmp KISSFFT_STATIC=1 KISSFFT_TOOLS=0 install
$ PKG_CONFIG_PATH=build-tmp/lib/pkgconfig meson setup builddir --buildtype=release
$ meson compile -C builddir
```

The final artifact would be `vsp` in `builddir`—takes no options, runs out of the box.

## Controls

- <kbd>↑</kbd> to increase, and <kbd>↓</kbd> to decrease gain.
- <kbd>←</kbd> to decrease, and <kbd>→</kbd> to increase smoothing time constant (0 < τ < 1).


### "Suckless" Approach

This app has no mechanism for dynamic configuration. If you want to tweak something, you can tweak it by *changing the constants* defined in `vsp.c`. It satirizes the cult-like philosophy Suckless.

Suppose you want to broaden the frequency range, you open `vsp.c` and modify `MAX_FREQ`—for example to `12000`.

```diff
-const float MAX_FREQ = 10000.0;
+const float MAX_FREQ = 12000.0;
```

And then,

```
$ meson compile -C builddir
```


### Smoothing time constant

Smoothing time constant (**τ**) is a parameter that controls how data is [smoothed exponentially](https://en.wikipedia.org/wiki/Exponential_smoothing) over time; a kind of lowpass filtering. In this context, it controls the responsiveness of the spectrum—higher the values, the smoother the animation. It defaults to `0.7`, which in my personal experience, provides the best balance of responsiveness and smoothness.


## Spectrum

The **spectrum is linear**, both on the frequency and the amplitude axis. The frequency range is limited between **50 to 10000 Hz**; chiefly considering visual aesthetics, because usually one generally won't be interested in the higher parts of the spectrum.

To understand the theory that underpins the mechanics of this program, i.e. **STFT** (Short-Time Fourier Transform), read [this](https://brianmcfee.net/dstbook-site/content/ch09-stft/intro.html) article; and, for the choice of normalization, read [this](https://appliedacousticschalmers.github.io/scaling-of-the-dft/AES2020_eBrief/#31--scaling-of-dft-spectra-of-discrete-tones) (rather technical) article.

## Why Rust to C?

As it is coded in C—an allegedly *"unsafe"* language—it is expected to be faster but more error-prone. But from personal experience, I can tell—it is the converse; because in Rust, one takes safety for granted, and when unsafe code hidden away by *"safe code"* malfunctions, it becomes a painstaking quest to identify the site of error and fix it.

Rust, as a modern language is complicated, and that too for a good reason; because *"primitive"* languages like C seem easy at the surface, but are actually hard to get right, especially when complicated things have to be built upon those primitive things. However, when it is about being certain of the exact behavior of a program, languages like Rust, that hide a lot of implementation detail beneath abstract *"zero-cost"* interfaces, make that process harder.

**TL;DR** The sheer complexity of the code made me frightened, that I won't able to comprehend any of it in future. Also, the compilation is too slow!

### OpenGL to `glium`

A while ago, I thought of replacing *"unsafe"* OpenGL calls with *"safe"* ones, so I approached `glium`. I learned, that the project has been discontinued by its owner and is maintained by the community; still I decided to adapt it. In those days I used **SDL**, and `glium` docs stated that any backend could be used if it [implemented the required traits](https://docs.rs/glium/latest/glium/backend/index.html). Needless to say, it took me quite a while to wrap my head around how to proceed, and after studying an existing implementation using **glutin** and **winit**, I eventually ended up with [this](https://github.com/cynthia2006/rvsp/blob/3f0da4c7f8f5a314e9e5d892c22df2903bc3684b/src/sdl_backend.rs) code.

### `winit`/`glutin`

Eventually, SDL's audio backend was replaced with PipeWire; subsequently, SDL was removed entirely with the introduction of the **`winit`/`glutin`** stack along with the code I wrote to adapt it for glium. It was quite a daunting task, because both **`winit`** and **`glutin`** lack tutorials describing their usage; involved mostly guesswork and experience. The only guiding example I could possibly find is [this](https://github.com/rust-windowing/glutin/blob/master/glutin_examples/src/lib.rs); *close the minimum* you require to draw an OpenGL triangle!

### `pipewire-rs`

The main motivation behind porting to C was the broken state of [pipewire-rs](https://gitlab.freedesktop.org/pipewire/pipewire-rs) library. It is largely unmaintained, and far behind in terms of compatibility with the [official C API](https://docs.pipewire.org/page_api.html) of PipeWire. The [support for **thread loops** is broken](https://gitlab.freedesktop.org/pipewire/pipewire-rs/-/issues/17) at best, as a consequence, I was forced to either [iterate](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.iterate) the [main-loop](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/main_loop/struct.MainLoop.html) along with **winit**'s event-loop, by hooking it in the [`about_to_wait()`](https://docs.rs/winit/latest/winit/application/trait.ApplicationHandler.html#method.about_to_wait) callback; or iterate it when an event was available on the [event loop's file descriptor](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.fd), as demonstrated [here](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/src/examples/gmain.c?ref_type=heads#L65-67). I opted for the first option, but felt it wasn't ideal, so I attempted the second option, but unfortunately **winit** [doesn't expose a method to add a file descriptor to be notified when events arrive on it](https://github.com/rust-windowing/winit/issues/3592).

Thus, came the conclusion to invert the paradigm. PipeWire [exposed such a method](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.add_io), so I instead added the [file descriptor of winit's event loop](https://github.com/rust-windowing/winit/blob/519947463fe2c2e213c5cc8f217554d07301ef23/src/event_loop.rs#L331-L333) (strangely, not enlisted in docs) to its watch list, waiting for events that arrive on it, as shown [here](https://github.com/rust-windowing/winit/blob/519947463fe2c2e213c5cc8f217554d07301ef23/src/event_loop.rs#L331-L333).

The issues don't just end here; when requested for mono audio ([here](https://github.com/cynthia2006/rvsp/blob/fb10b69fa57e7db77a228d6d550ed15105da1713/src/main.rs#L369-L376)) through `info.set_channels(1)`, it only links the left channel, ignoring the right entirely as shown in the picture. 

![2025-05-06_15-55](https://github.com/user-attachments/assets/3354c5de-a14b-4c66-8e6c-38b8577514e9)


It should downmix these channels, but since for a strange reason it doesn't, so [I have to implement it myself](https://github.com/cynthia2006/rvsp/blob/fb10b69fa57e7db77a228d6d550ed15105da1713/src/main.rs#L101-L111). However, using the official C API, this issue is resolved automatically.

### `glium` to OpenGL

Since `glium` is not maintained anymore, I thought of returning to OpenGL; also because `glium`'s abstraction of OpenGL felt too alienating from the thing it attempted to abstract, OpenGL.
