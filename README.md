# vsp

vsp is a lightweight OpenGL audio visualizer, which captures your system audio and displays the spectrum. The yellow-on-black colour scheme is a part of its heritage. It's a port of [rvsp](https://github.com/cynthia2006/rvsp), but improved in many ways. 

[CinematicVSP.webm](https://github.com/user-attachments/assets/c3e6a155-3f7c-4178-bced-53face6879ce)

<div align="center">(Music: <i>Meltt — Within You, Within Me</i>)</div> 

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

- <kbd>↑</kbd> to increase and <kbd>↓</kbd> to decrease gain of the spectrum.
- <kbd>←</kbd> to decrease and <kbd>→</kbd> to increase smoothing time constant (0 < τ < 1).


### "Suckless" approach

This app is as bare minimum as it could get, and the defaults are universally the best choice. However, if your needs are special you can of course adjust the options by editing the code itself (options in `vsp.c`). There is no mechanism for loading configuration files, as the code needed for that would alone outweigh the existing codebase. In the Linux community this is known as the "suckless" approach.


### Smoothing time constant

Smoothing time constant (**τ**) is a parameter controlling [temporal smoothing of spectrum](https://en.wikipedia.org/wiki/Exponential_smoothing); higher the values the smoother the animation. The default is 0.8, which is quite eye-pleasing; lower values (typically around 0.6) are good for high-BPM music if you're into that.

## Spectrum

A [Mel-scale](https://en.wikipedia.org/wiki/Mel_scale) spectrum (20-20000 Hz) is used to display lower frequencies in greater detail, and higher frequencies in coarse detail. Amplitude is in linear scale however, simply because it's visually appealing. You usually adjust the gain of the spectrum using the <kbd>↑</kbd> and <kbd>↓</kbd> keys as needed (e.g. the volume of music is too low).

## Why Rust to C?

`vsp` is rewritten in C—an allegedly ‘unsafe’ language. When I actively worked on `rvsp`, I had a strange a epiphany that Rust programmers often take safety for granted. A seemingly safe interface when it has a logical error it could even be harder to trace than undefined behaviour. And, if the safe interface is the mould of an unsafe interface, a logical error within the implementation of the safe code creates the illusion of safety, and thus the concept of fearlessness Rust boasts about is tested. Ofcourse, as a ‘safe’ language Rust places various barriers in form of ownership and borrow checkers, ensuring no incorrect safe code can compile; in the worst case, crash without inducing undefined behaviour (e.g. out of bounds indexing)—all of this you already know however. My issue here is with a certain class of Rust code: bindings.

As it happens so often, that majority of libraries haven't yet been ‘rewritten to Rust’, thus the need for bindings (using FFI). Rust is particularly ugly with C interop. I call it ‘pathetic’. Bindings crates exist so that the average Rust user can overcome this ugliness. To me ‘unsafe Rust’ is the same as ‘unsafe sex’—two dangerous thrills of life. Often the documentation of safe interfaces is non-existent or poor, and sometimes the ‘safe’ interface malfunctions in a non-deterministic way. It then becomes a necessity to dive into the actual source code to understand what it is doing with the underlying C library to ascertain correct behaviour; this extra effort could just be put into directly using the ‘unsafe’ interface than understand the nook and corner of the ‘safe’ interface. This is also what is wrong with C++; instead of becoming a supposedly ‘better’ language than C, it brings it own deficiencies to become nothing but a cancerous disease.

Enough rambling, I guess. The reason for me to move away from `rvsp` to create `vsp` is chiefly because of the trashy `pipewire-rs` library, and because of the complexities introduced by `winit` and `glutin`—all to just initialise a simple OpenGL context. As a hobby project, I wasn't enjoying coding it; rather I was anxious to navigating through the needless complexities, sometimes staying late awake at night. Another benefit of moving away from Rust to C was instant compilation :)

### OpenGL to `glium`

Originally, I had used `skia-rs` as the 2D drawing library. It was a heavy dependency taking an excessively long-time to compile, because it compiled Skia from source. I had to struggle setting up the Ganesh backend of Skia for OpenGL accelerated rendering. The example code using `winit` and `glutin` (libraries whom I had no awareness of then) made it worse, for I had to change those examples to work with **SDL**. Eventually I learnt basics of OpenGL, and replaced Skia with ‘unsafe’ OpenGL calls (imagine how ‘pathetic’ graphics development is in Rust). The code worked as expected, fortunately.

I grew paranoid over ‘unsafe’ OpenGL calls, and as a good Rust user, had wanted to replaced them with ‘safe’ ones. I stumbled upon `glium`. The project had long been discontinued by its author (Tomaka), and was under community maintenance. I still decided to try it out. I used **SDL** then, but `glium` only had seamless integration with `winit` and `glutin`. The documentation stated that any backend could be used only if it [implemented the required traits](https://docs.rs/glium/latest/glium/backend/index.html). It cost me an embarrasingly long time to figure out how the traits are meant to interact with each other, prompting me to study the code from `glium`, and then figure out how to integrate all that with SDL. I ended up with [this code](https://github.com/cynthia2006/rvsp/blob/3f0da4c7f8f5a314e9e5d892c22df2903bc3684b/src/sdl_backend.rs), and it worked.

### `winit` + `glutin`

SDL's audio interface was inadequate for my needs from the start, and it was merely kept for portability across platforms. I manually had to route the system audio to the program through **pavucontrol**, otherwise it captured my from the microphone; when **pavucontrol** broke for some reason, through **qpwgraph** (extremely unreliable). I opted for PipeWire, sacrificing portability to systems other than Linux.

I still used `glium`; so to remove the SDL dependency entirely, I brought in **winit** and **glutin**. The code became moderately complicated, but still within grasp.

### `glium` to OpenGL

Because `glium` was not actively developed anymore, and because it was too abstract, I thought of doing away with it. I wish, I had been aware of what I was signing up for. I would either leave it at that or rewrite it in C. It was quite exhausting as both `winit` and `glutin` sorely lack tutorials to describe their intended usage. I had to rely on mostly guesswork and experience, and the only guiding example I could possibly find is [this](https://github.com/rust-windowing/glutin/blob/master/glutin_examples/src/lib.rs), which apparently is the minimum you require to draw an OpenGL triangle!

### `pipewire-rs`

The primary reason behind this C rewrite was the absolutely garbage [pipewire-rs](https://gitlab.freedesktop.org/pipewire/pipewire-rs) library—largely unmaintained, and not on par with development of the [official C API](https://docs.pipewire.org/page_api.html) of PipeWire. The [support for **thread loops** is severely broken](https://gitlab.freedesktop.org/pipewire/pipewire-rs/-/issues/17), which cause programs to unexpectedly crash. As a consequence, I was forced to either [iterate](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.iterate) the mainloop through hooking it in the [`about_to_wait()`](https://docs.rs/winit/latest/winit/application/trait.ApplicationHandler.html#method.about_to_wait) callback, or to iterate it when an event was available on the [event loop's file descriptor](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.fd) (as demonstrated [here](https://gitlab.freedesktop.org/pipewire/pipewire/-/blob/master/src/examples/gmain.c?ref_type=heads#L65-67)). Initially I went for the first option, but the main problem with that approach was that, frequency of iterations of `winit`'s event loop, never ought to match the frequency of iterations in PipeWire's event loop; in other words—winit's eventloop not running would stall PipeWire's eventloop, and blocking it from processing events received on its own fd (file descriptor): a suboptimal solution. Thus, I tried to adopt the second option, but unfortunately `winit` (as of now) doesn't expose a method to [add a fd to its watchlist and notify when events arrive on it](https://github.com/rust-windowing/winit/issues/3592).

PipeWire however [had such a method](https://pipewire.pages.freedesktop.org/pipewire-rs/pipewire/loop_/struct.LoopRef.html#method.add_io), thus I instead added the [fd of winit's event loop](https://github.com/rust-windowing/winit/blob/519947463fe2c2e213c5cc8f217554d07301ef23/src/event_loop.rs#L331-L333) (strangely, not enlisted on **docs.rs** of `winit`) to its watch list (as shown [here](https://github.com/rust-windowing/winit/blob/519947463fe2c2e213c5cc8f217554d07301ef23/src/event_loop.rs#L331-L333)).

The issues don't just end here! Requests for mono-audio ([here](https://github.com/cynthia2006/rvsp/blob/fb10b69fa57e7db77a228d6d550ed15105da1713/src/main.rs#L369-L376)) through `info.set_channels(1)` only links the left channel, ignoring the right entirely (as shown in this screenshot; qpwgraph).

![2025-05-06_15-55](https://github.com/user-attachments/assets/3354c5de-a14b-4c66-8e6c-38b8577514e9)


The C API downmixes the channels appropriately, but `pipewire-rs` binding for a strange reason, doesn't; so [I had to implement it myself](https://github.com/cynthia2006/rvsp/blob/fb10b69fa57e7db77a228d6d550ed15105da1713/src/main.rs#L101-L111). And, this last issue was enough for me to drive away from development of **rvsp**.
