# LibreTiny H-Bridge Light Component

Because [LibreTiny doesn't currently provide a way to do synchronous PWM](https://github.com/libretiny-eu/libretiny/issues/224), I decided to hack it in on my own.  
This is needed for these cheap fairy light strings that only have two wires and the LEDs wired anti-parallel.

It's currently only tested on the Beken BK7231**N** MCU.

It could've been implemented in a more proper way, but this is left as an exercise to the reader.  
(Enforcing the different channels via config, making the frequency adjustable, BK7231U/T support, making it more general, etc.)