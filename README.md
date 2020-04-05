# audiomux
Audio Multiplexing Framework
============================

After getting desperate to find a solution for low-latency and high-quality audioconferencing for remote rehearsals, I decided to start my own project. I don't know yet, if this will work.

Target is to have 48kHz Stereo-Audio with many tiers, which are routed through a central UDP server combining and correcting the signal delays. 
The only usable software up to now is Jamkazaam, however we regulary fail to have a decent audio quality and the configuration is too feature rich = complicated.
WIth an 800kBit upload connectivity I never managed to connect more than one person. 


The project is still in prototype status and is built using:
* the Opus codec library
* the portaudio library


