# vsp

vsp is a lightweight OpenGL audio visualizer. A port of [rvsp](https://github.com/cynthia2006/rvsp), the functionality is an exact replica of the original, save for a few things. 

## Installation

### Prerequisites

- Meson (build system)
- CMake (for Meson; more specifically, for KissFFT)
- GLFW 3.0
- PipeWire ≥0.3

```
$ git clone --recurse-submodules https://github.com/cynthia2006/vsp
$ cd vsp
$ meson setup builddir --buildtype=release
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

`vsp` is rewritten in C, an allegedly “unsafe” language. However, during development of `rvsp`, I was confronted with a strange a epiphany, that Rust programmers, often—if not always—take safety for granted. A safe interface, when it incorrectly acts with an unsafe interface, malfunctions, it becomes a painstaking job of identifying (mostly identifying) and fixing the site of error.

As a supposed “safe” language, Rust places various barriers, so as to ensure that no incorrect safe code can compile; or in the worst case, crash without inducing undefined behaviour. The apparent simplicity of C is quite decieving, because historically, it's lack of safety checks has resulted in a large number of CVEs related to memory safety issues (buffer overflows, race conditions, etc.) All of this you already know. However, my issue here is with a certain class of Rust code, or rather say, library crates—bindings.

As it happens so often, most of the existing libraries haven't already been “rewritten to Rust”, and thus we require bindings to them using FFI. One of the very displeasing aspects of Rust is C interoperatibility; it can summarised as “pathetic” in my opinion, and to hide this “pathetic” detail, “safe” interfaces are often written on top of “unsafe” ones (the FFI), simply because “unsafe Rust” is like “unsafe sex”—both are very dangerous, and both suck. Sometimes, the documentation of safe interfaces is non-existent or is poor; and sometimes, the “safe” interface malfunctions in a non-deterministic way, so it becomes a necessity to dive into the actual source code, to understand what library calls it actually performs to ascertain correct behaviour—this extra effort could just be channelised into directly using the “unsafe” interface, rather than understand the “safe” interface. This is also what is wrong with C++. Instead of becoming a supposedly “better” language than C, it adds on top of C's defecits with its own, becoming nothing but a cancerous disease that everyone pretends to praise, but internally loathes.

The reason I moved away from `rvsp` to create `vsp`, is mainly because of trashy `pipewire-rs`, and because of the complexities introduced by `winit` and `glutin` pair just to initialise a simple OpenGL context! My faint heart, which adores simplicity, was frightened by the monster I had created in the name of “safe” code. Another apparent benefit of moving from Rust to C was faster compilation times; it was important for me, because on my computer, and especially Raspberry Pis, it takes a significant amount of time to compile the code, as it first fetches the dependencies from **crates.io** them compiles them. This is another large minus point for Rust, as there's no stable ABI yet.

### OpenGL to `glium`

Originally, I had used `skia-rs` as the 2D drawing library. It was a heavy dependency, and took an excessively long-time to compile because it compiled the C++ code (language Skia is written in) as well; it was hard to set up as well, especially its GPU accelarated backend (OpenGL). The example code using `winit` and `glutin` (libraries whom I had no awareness of) made it worse, because I had to change those examples, so that they work with **SDL**. Eventually, I learnt basics of OpenGL, then subsequently replacing Skia with “unsafe” OpenGL calls (you could already imagine how “pathetic” graphics development would be in Rust). Fortunately, the code worked as expected.

Then, one day, I felt a wanton urge of replacing “unsafe” OpenGL calls with “safe” ones, thus I search around and approached `glium`. Unfortunately, the project had long been discontinued by its author, and was being maintained by the community; however, I still decided to try it out. I used **SDL**, but `glium` by default had integration with `winit` and `glutin`; the docs however stated, that any backend could be used, if it [implemented the required traits](https://docs.rs/glium/latest/glium/backend/index.html). It took me an embarrasingly long time, first understand how to interfaces are meant to interact with each other, requiring to study the existing implementation for `winit` and `glutin`; then understanding how to integrate with SDL. In the end, ended up with [this code](https://github.com/cynthia2006/rvsp/blob/3f0da4c7f8f5a314e9e5d892c22df2903bc3684b/src/sdl_backend.rs); and it worked.

### `winit`/`glutin`

SDL's audio backend was inadequate for my needs from the start; it was merely kept for portability reasons. I had to manually, configure the capture source to be the monitor of system audio sink, instead of my microphone, through **pavucontrol**; then when **pavucontrol** broke for some reason, through **qpwgraph** (extremely unreliable). It was now time to sacrifice portability, and make it Linux only; mainly because Linux is my main workstation. SDL's audio backend was replaced with PipeWire.

I still used `glium`, so to ditch the SDL dependency entirely, I opted for the more compatible **winit** and **glutin** pair. The code as a result, become a bit complicated, but not as much.

### `glium` to OpenGL

Because `glium` was not actively developed anymore, and because its interface was a far cry from what is meant to abstract, OpenGL, I thought of ditching it. Had I known the sea of complexities I would find myself in, I would either leave it at that, or start the work on a C rewrite that point on words. It was a daunting task, because both `winit` and `glutin`  lack any tutorials to describe their intended usage. I had to rely on mostly guesswork and experience, and the only guiding example I could possibly find is [this](https://github.com/rust-windowing/glutin/blob/master/glutin_examples/src/lib.rs), which apparently is the minimum you require to draw an OpenGL triangle!

### `pipewire-rs`

The prime reason behind a C rewrite was the absolutely trash [pipewire-rs](https://gitlab.freedesktop.org/pipewire/pipewire-rs) library—largely unmaintained, and not on par with development of the [official C API](https://docs.pipewire.org/page_api.html) of PipeWire. The [support for **thread loops** is severely broken](https://gitlab.freedesktop.org/pipewire/pipewire-rs/-/issues/17), causing program crashes. As a consequence, I was forced to either [iterate](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.iterate) the main-loop through hooking it in the [`about_to_wait()`](https://docs.rs/winit/latest/winit/application/trait.ApplicationHandler.html#method.about_to_wait) callback; or, to iterate it when an event was available on the [event loop's file descriptor](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.fd), as demonstrated [here](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/src/examples/gmain.c?ref_type=heads#L65-67). Initially, I opted for the first option; but the main problem with that approach was that, frequency of iterations of `winit`'s event loop, never ought to match the frequency of iterations in PipeWire's event loop; in other words, winit's event-loop not running would stall PipeWire's event-loop, and forbid it from processing events recieved on its own file descriptor. This would be a suboptimal solution, so I tried to implement the second option, but unfortunately `winit`—as of now—doesn't expose a method to [add a file descriptor to be notified when events arrive on it](https://github.com/rust-windowing/winit/issues/3592), unlike glib's event-loop.

Thus, the consensus was to invert the paradigm. PipeWire [exposed such a method](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.add_io), so I instead added the [file descriptor of winit's event loop](https://github.com/rust-windowing/winit/blob/519947463fe2c2e213c5cc8f217554d07301ef23/src/event_loop.rs#L331-L333) (strangely, not enlisted on **docs.rs**) to its watch list as shown [here](https://github.com/rust-windowing/winit/blob/519947463fe2c2e213c5cc8f217554d07301ef23/src/event_loop.rs#L331-L333).

The issues don't just end here; when requested for single-channel audio ([here](https://github.com/cynthia2006/rvsp/blob/fb10b69fa57e7db77a228d6d550ed15105da1713/src/main.rs#L369-L376)) through `info.set_channels(1)`, it only links the left channel, ignoring the right entirely, as shown in this picture.

![2025-05-06_15-55](https://github.com/user-attachments/assets/3354c5de-a14b-4c66-8e6c-38b8577514e9)


Following the behaviour of the C API, it should downmix these channels, but since for a strange reason it doesn't, so [I have to implement it myself](https://github.com/cynthia2006/rvsp/blob/fb10b69fa57e7db77a228d6d550ed15105da1713/src/main.rs#L101-L111).
