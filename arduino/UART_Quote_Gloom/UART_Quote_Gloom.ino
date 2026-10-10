/*
  UART_Quote_Gloom.ino - gloomy "quote of the day" for the Sharp PC-1500(A) with
  SERINOUT v7.2 (Software UART), 19200 bps, Arduino UNO: grotesque little
  catastrophes from IT, electronics and space. The LCD Keypad Shield is optional.

  The PC-1500 program pc1500_uart_quote-v1.0.txt (the same as for UART_Quote)
  asks, in a loop and at random,
      What's for today?   /   What's for tomorrow?   /   What's for yesterday?
  (ended with CR) and shows each answer for about 8 s. The UNO builds a sentence
  from the word lists below in one of four patterns, in the present (today),
  future (tomorrow) or past tense (yesterday):
      <subject> <verb> <object>.      A consultant ate the LPTs.
      <subject> <verb> <place>.       The UART sang in water.
      <subject> <verb> <adverb>.      Diodes danced gloomily.
      <Adverb> <subject> <verb>.      Mournfully the HDD worked.
  at most 26 characters (one line of the PC-1500 display), and sends it back,
  filled up with spaces to 26 characters and followed by CR (always 27
  characters, so CALL SI,M with M = 27 returns at once).

  Wiring (the same as for UART_Quote and the calibration; everything at 5 V):
    PC-1500 PC7 (TX)  pin 10    ---|<|---  A5   Schottky diode, cathode (stripe) to PC7,
                                           or a 1 kOhm resistor
    PC-1500 PB2 (RX)  pin 27    --[470]--  A4
    PC-1500 GND       pin 52-55 --------  GND
    optional: 4.7 kOhm from A5 to 5 V (faster rising edges with the diode)
  The PC-1500 must be out of the CE-150 (PB2 is its cassette input).

  On the PC-1500: installer v7.2 with speed 5 (19200), RX port PB2, INVERSION
  no; then NEW and the quote program. Other speeds: change BAUD below and
  install the same speed on the PC-1500.

  The serial monitor (115200 bps) shows every question and answer.
*/
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>
#include <ctype.h>

#define BAUD 19200       // the speed installed on the PC-1500 (1200 .. 19200)
#define LINE_LEN 26      // characters in one line of the PC-1500 display
#define ANSWER_DELAY 100 // ms: the PC-1500 goes from CALL SO to CALL SI meanwhile

enum Tense : uint8_t { PRESENT, FUTURE, PAST, UNKNOWN };

// ==== quote generator begin (no hardware access; the same code is compiled
// ==== on a PC for tests)

// Word lists in flash (404 subjects, 171 + 157 verbs, 133 adverbs, 148 places, 503 objects),
// items separated by '|'. Add or change words as you like: the sketch takes
// about 28 KB of the 32 KB flash of the UNO - keep some of it free. A
// sentence longer than LINE_LEN is never sent, another one is chosen.

// Subjects as in the middle of a sentence ("the CPU", "a diode", "Mars"); the
// first letter becomes capital at the start of a sentence. '+' = plural
// ("+diodes": "diodes dance", not "dances"). Up to 16 characters.
const char SUBJECTS[] PROGMEM =
  "the UART|the CPU|the GPU|the ALU|the FPU|the HDD|the SSD|the floppy drive|the modem|the router|"
  "the switch|the server|the mainframe|the PC-1500|the printer|the plotter|the scanner|"
  "the keyboard|the mouse|the monitor|the CRT|the LCD|the PSU|the UPS|the battery|the BIOS|"
  "the kernel|the compiler|the linker|the debugger|the assembler|the interpreter|the stack|"
  "the heap|the cache|the data bus|the clock|the watchdog|the interrupt|a null pointer|"
  "a stray pointer|a bug|a virus|a worm|a trojan|a deadlock|a segfault|a kernel panic|the firewall|"
  "the cloud|the database|the backup|the intern|a consultant|the sysadmin|the admin|the manager|"
  "the CTO|the CEO|a hacker|a coder|a tester|the helpdesk|a robot|the AI|a chatbot|the algorithm|"
  "the satellite|the spacecraft|the rocket|the space probe|the rover|the lander|the shuttle|"
  "the astronaut|a cosmonaut|an alien|the moon|Mars|Jupiter|Saturn|Pluto|a comet|a meteor|"
  "an asteroid|a black hole|a quasar|a pulsar|a nebula|the sun|a supernova|Houston|mission control|"
  "the oscilloscope|the multimeter|the breadboard|the Arduino|the EPROM|the ROM|the RAM|"
  "a lonely byte|a lost bit|a dead pixel|the cursor|the spacebar|the Enter key|the reset button|"
  "the fan|the heatsink|the motherboard|the chipset|the oscillator|the crystal|the antenna|"
  "the radar|the laser|the cable|the jumper|the socket|the plug|the transformer|the amplifier|"
  "the speaker|the buzzer|the cassette|the tape drive|the punch card|the teletype|the terminal|"
  "the shell|the daemon|a thread|a process|a diode|a transistor|a capacitor|a resistor|an inductor|"
  "a relay|a fuse|an LED|a thermistor|a vacuum tube|a solder joint|a soldering iron|an electron|"
  "a photon|a neutrino|a qubit|the Wi-Fi|the hotspot|the password|the spam filter|the inbox|"
  "the spreadsheet|the scrollbar|the progress bar|the hourglass|the blue screen|the error log|"
  "the stack trace|the hotfix|the update|the patch|the release|the deadline|the budget|the project|"
  "the sprint|the meeting|the webinar|the paper jam|the toner|the scrum master|the architect|"
  "the DBA|the dev team|the QA team|the night shift|the janitor bot|the coffee maker|the microwave|"
  "the toaster|the smart fridge|the smartwatch|the drone|the 3D printer|the VR headset|the webcam|"
  "the microphone|the headset|the joystick|the trackball|the light pen|the dot matrix|"
  "the daisy wheel|the modem tone|the dial tone|the handshake|the parity bit|the stop bit|"
  "the start bit|the baud rate|the buffer|the checksum|the semicolon|the bracket|the null byte|"
  "the zero flag|the carry flag|the accumulator|the register|the opcode|the bootloader|"
  "the firmware|the driver|the OS|the GUI|the CLI|the API|the SDK|the IDE|the cookie|the session|"
  "the token|the cron job|the macro|the script|the loop|the recursion|the exception|the warning|"
  "the crash dump|the core dump|the swap file|the page fault|the tilde|the hash sign|"
  "the comet tail|the launch pad|the airlock|the space suit|the solar panel|the oxygen tank|"
  "the space toilet|the moon base|the Mars rover|the star map|the telescope|the radio dish|"
  "the countdown|the escape pod|the warp drive|the tractor beam|a wormhole|a red giant|"
  "a white dwarf|a brown dwarf|a gas giant|an ice moon|a lost probe|an old satellite|a bored alien|"
  "a sad robot|a tired intern|a drunk server|a lonely modem|a grumpy CPU|a weeping diode|"
  "a moody router|a broken fan|a burnt fuse|a cracked screen|a dead battery|a rusty relay|"
  "a haunted laptop|a cursed floppy|a zombie process|a rogue AI|a mad scientist|a lost astronaut|"
  "a space pirate|a sad cosmonaut|a nervous rocket|+diodes|+transistors|+capacitors|+resistors|"
  "+electrons|+photons|+neutrinos|+qubits|+bits|+bytes|+pixels|+cables|+fans|+the servers|"
  "+the routers|+the engineers|+the interns|+the testers|+the robots|+the aliens|+the astronauts|"
  "+the satellites|+the asteroids|+the stars|+the planets|+punch cards|+floppies|+cosmic rays|"
  "+solar flares|+gremlins|+bugs|+the LEDs|+the relays|+the fuses|+the consultants|+the managers|"
  "+the hackers|+the coders|+the users|+the admins|+the drones|+the rovers|+the comets|+Martians|"
  "+the moons|+the cookies|+the packets|+packets|+the threads|+the macros|+the logs|+the backups|"
  "+the tapes|+the floppies|+the CRTs|+vacuum tubes|+zombie threads|+the electrons|+the semicolons|"
  "+the brackets|+the pointers|+lost packets|+dead pixels|+broken links|+the sysadmins|"
  "+the QA testers|+the cosmonauts|+meteorites|+black holes|+the quasars|+space rocks|+the rockets|"
  "+the probes|+the landers|the dial-up|the COM port|the LPT port|the IRQ|the DMA|a bad sector|"
  "the boot sector|the registry|a core dump|a device driver|the address bus|the 555 timer|"
  "an op-amp|a zener diode|a stepper motor|a servo|a Geiger counter|a Tesla coil|the power grid|"
  "the server rack|the KVM switch|a blade server|the tape robot|the air con|a robot vacuum|"
  "a smart bulb|the thermostat|the Hubble|the moon buggy|the lunar module|a lost wrench|"
  "a space sock|a sunspot|a solar flare|the solar wind|the ozone layer|+the sunspots|+cosmic eggs|"
  "+the servos|+stepper motors|+the op-amps|+zener diodes|+bad sectors|+the moon buggies|"
  "+lost wrenches|+the IRQs";

// Verbs: "base form,past tense"; also with a preposition ("crash into,crashed
// into"). The present form is made by rule from the first word: -s, -es
// (s x z o ch sh) or -ies (consonant + y); avoid "have" and "be".
// Transitive verbs (with an object):
const char VERBS_T[] PROGMEM =
  "eat,ate|swallow,swallowed|devour,devoured|crush,crushed|smash,smashed|melt,melted|fry,fried|"
  "burn,burned|nuke,nuked|delete,deleted|format,formatted|corrupt,corrupted|overwrite,overwrote|"
  "reboot,rebooted|crash,crashed|hack,hacked|infect,infected|brick,bricked|zap,zapped|"
  "unplug,unplugged|drop,dropped|flood,flooded|drown,drowned|bury,buried|mourn,mourned|"
  "haunt,haunted|curse,cursed|sue,sued|fire,fired|bite,bit|lick,licked|chew,chewed|sniff,sniffed|"
  "kiss,kissed|hug,hugged|marry,married|divorce,divorced|adopt,adopted|rob,robbed|kidnap,kidnapped|"
  "dissolve,dissolved|vaporize,vaporized|disassemble,disassembled|solder,soldered|"
  "desolder,desoldered|overclock,overclocked|sell,sold|pawn,pawned|lose,lost|misplace,misplaced|"
  "forget,forgot|launch,launched|abduct,abducted|orbit,orbited|colonize,colonized|invade,invaded|"
  "irradiate,irradiated|magnetize,magnetized|debug,debugged|compile,compiled|decompile,decompiled|"
  "encrypt,encrypted|decrypt,decrypted|leak,leaked|spam,spammed|mine,mined|ping,pinged|"
  "flush,flushed|toast,toasted|microwave,microwaved|grill,grilled|salt,salted|pickle,pickled|"
  "worship,worshipped|bless,blessed|insult,insulted|ignore,ignored|betray,betrayed|"
  "abandon,abandoned|outsource,outsourced|rebrand,rebranded|refactor,refactored|"
  "deprecate,deprecated|patch,patched|swap,swapped|upload,uploaded|download,downloaded|"
  "email,emailed|fax,faxed|print,printed|staple,stapled|tape,taped|glue,glued|rewire,rewired|"
  "invert,inverted|amplify,amplified|ground,grounded|short,shorted|blow,blew|jam,jammed|"
  "clog,clogged|rust,rusted|dunk,dunked|bake,baked|boil,boiled|stew,stewed|blend,blended|"
  "shred,shredded|crumple,crumpled|chop,chopped|tickle,tickled|lasso,lassoed|hypnotize,hypnotized|"
  "serenade,serenaded|interrogate,interrogated|baptize,baptized|knight,knighted|crown,crowned|"
  "exile,exiled|banish,banished|summon,summoned|resurrect,resurrected|embalm,embalmed|"
  "mummify,mummified|fossilize,fossilized|petrify,petrified|clone,cloned|spoon-feed,spoon-fed|"
  "inhale,inhaled|sneeze on,sneezed on|weep over,wept over|cry over,cried over|sing to,sang to|"
  "pray to,prayed to|spit on,spat on|step on,stepped on|sit on,sat on|trip over,tripped over|"
  "stare at,stared at|scream at,screamed at|bark at,barked at|laugh at,laughed at|"
  "growl at,growled at|howl at,howled at|dream of,dreamed of|long for,longed for|"
  "grieve for,grieved for|vote for,voted for|pay for,paid for|search for,searched for|"
  "wait for,waited for|hide from,hid from|run from,ran from|argue with,argued with|"
  "flirt with,flirted with|dance with,danced with|elope with,eloped with|merge with,merged with|"
  "collide with,collided with|crash into,crashed into|bump into,bumped into|turn into,turned into|"
  "morph into,morphed into|melt into,melted into|fall for,fell for|lean on,leaned on|"
  "feast on,feasted on|choke on,choked on|nibble on,nibbled on|snack on,snacked on|spy on,spied on";

// Intransitive verbs (with an adverb or a place):
const char VERBS_I[] PROGMEM =
  "die,died|weep,wept|cry,cried|sob,sobbed|sigh,sighed|groan,groaned|moan,moaned|scream,screamed|"
  "howl,howled|sing,sang|dance,danced|waltz,waltzed|hum,hummed|buzz,buzzed|beep,beeped|"
  "crash,crashed|freeze,froze|hang,hung|reboot,rebooted|overheat,overheated|melt,melted|"
  "explode,exploded|implode,imploded|collapse,collapsed|evaporate,evaporated|rust,rusted|"
  "decay,decayed|fade,faded|flicker,flickered|glow,glowed|spark,sparked|smoke,smoked|"
  "sizzle,sizzled|fizzle,fizzled|burn,burned|drown,drowned|sink,sank|fall,fell|tumble,tumbled|"
  "wobble,wobbled|drift,drifted|float,floated|orbit,orbited|spin,spun|crawl,crawled|limp,limped|"
  "stumble,stumbled|sleep,slept|snore,snored|dream,dreamed|work,worked|fail,failed|resign,resigned|"
  "retire,retired|panic,panicked|surrender,surrendered|vibrate,vibrated|oscillate,oscillated|"
  "leak,leaked|drip,dripped|sneeze,sneezed|cough,coughed|hiccup,hiccuped|yawn,yawned|pray,prayed|"
  "wait,waited|complain,complained|shrink,shrank|swell,swelled|mutate,mutated|vanish,vanished|"
  "hibernate,hibernated|wander,wandered|crash-land,crash-landed|blink,blinked|stutter,stuttered|"
  "lag,lagged|time out,timed out|give up,gave up|boot up,booted up|shut down,shut down|"
  "burn out,burned out|black out,blacked out|freak out,freaked out|pass out,passed out|"
  "break down,broke down|melt down,melted down|go dark,went dark|go silent,went silent|rot,rotted|"
  "mourn,mourned|grieve,grieved|despair,despaired|sulk,sulked|pout,pouted|brood,brooded|"
  "whimper,whimpered|wail,wailed|shiver,shivered|tremble,trembled|glitch,glitched|"
  "overflow,overflowed|underflow,underflowed|segfault,segfaulted|deadlock,deadlocked|fork,forked|"
  "halt,halted|idle,idled|thrash,thrashed|levitate,levitated|teleport,teleported|"
  "disintegrate,disintegrated|combust,combusted|ignite,ignited|corrode,corroded|"
  "short out,shorted out|fall apart,fell apart|crumble,crumbled|wilt,wilted|drool,drooled|"
  "giggle,giggled|cackle,cackled|weep softly,wept softly|meow,meowed|quack,quacked|yodel,yodeled|"
  "tap-dance,tap-danced|moonwalk,moonwalked|breakdance,breakdanced|hide,hid|sweat,sweated|"
  "bleep,bleeped|crackle,crackled|hiss,hissed|rattle,rattled|squeak,squeaked|ping,pinged|"
  "buffer,buffered|compile,compiled|boot,booted|spiral,spiraled|plummet,plummeted|hover,hovered|"
  "glide,glided|bounce,bounced|wiggle,wiggled|jiggle,jiggled|twitch,twitched|flinch,flinched|"
  "sneer,sneered|whine,whined|grumble,grumbled|mumble,mumbled|babble,babbled|chant,chanted|"
  "rap,rapped|tango,tangoed";

// Adverbs, also at the start of a sentence: up to 16 characters.
const char ADVERBS[] PROGMEM =
  "gloomily|mournfully|sadly|grimly|tragically|bitterly|slowly|silently|loudly|wildly|madly|"
  "morosely|wearily|hopelessly|helplessly|horribly|terribly|awfully|darkly|dismally|miserably|"
  "woefully|forlornly|glumly|sullenly|sheepishly|nervously|anxiously|desperately|frantically|"
  "feverishly|absurdly|grotesquely|bizarrely|oddly|strangely|eerily|quietly|endlessly|forever|"
  "again|in vain|in silence|in despair|in tears|in binary|in hex|in ASCII|in Morse code|"
  "at midnight|at dawn|on Monday|on Friday|all night|all weekend|once more|too early|too late|"
  "way too late|backwards|upside down|in slow motion|with a sigh|without a sound|for no reason|"
  "like a toaster|like a potato|clumsily|awkwardly|pathetically|dramatically|theatrically|"
  "operatically|for hours|for eternity|in reverse|in a loop|in parallel|in assembler|in COBOL|"
  "in BASIC|in Fortran|in Latin|off-key|out of tune|in 8 bits|at 300 baud|at 19200 baud|"
  "without backup|without warning|on schedule|behind schedule|ahead of time|quite sadly|"
  "very slowly|rather loudly|gracelessly|mechanically|digitally|electrically|magnetically|"
  "radioactively|cosmically|atomically|recursively|asynchronously|synchronously|randomly|serially|"
  "bit by bit|byte by byte|line by line|frame by frame|at full speed|at low power|on battery|"
  "on backup power|with a beep|with a crash|with a bang|with a pop|with a fizz|with a whimper|"
  "in a panic|in a hurry|at last|too soon|in style|in stereo|in mono|in 4K|in grayscale|in sepia";

// Places: up to 16 characters.
const char PLACES[] PROGMEM =
  "in water|in the rain|in the sea|in the cloud|in the cache|in the stack|in the trash|in a ditch|"
  "in orbit|in space|in a black hole|on Mars|on the moon|on Pluto|under the desk|under the sea|"
  "inside the CPU|into pizza|into the sun|into a pizza|into soup|into lava|into a puddle|"
  "into the void|into /dev/null|through the wall|over the router|near the fridge|during a demo|"
  "during backup|after the update|before launch|at the launch|in the basement|in the attic|"
  "in the fridge|in the microwave|in a toaster|in a teapot|in the coffee|in the soup|in a sock|"
  "in a drawer|in the wiring|in the firmware|in the kernel|in the BIOS|in /tmp|on the floor|"
  "on the roof|on the desk|on the keyboard|on a floppy|on the ISS|on Venus|on the dark side|"
  "off the grid|out of memory|out of orbit|out of range|out of order|in flames|in the sandbox|"
  "in production|in the logs|in debug mode|in safe mode|in the dark|in a crater|in zero-g|"
  "at warp speed|near Jupiter|past Saturn|beyond Pluto|around the moon|around a pizza|"
  "under the bed|inside a cake|inside a donut|inside a burger|on a pizza|in a spreadsheet|"
  "in a meeting|in a webinar|at the helpdesk|on hold|in the queue|in the backlog|in a time loop|"
  "in a wormhole|in the airlock|on the rings|on a comet|on an asteroid|in a moon crater|"
  "in the vacuum|in a capacitor|in a diode|on a breadboard|in the solder|under the PCB|"
  "in the printer|in the toner|on the scanner|in the modem|in the router|in the mainframe|"
  "on a punch card|on tape|on a cassette|in the dial-up|on the COM port|in a bad sector|"
  "in the registry|in a core dump|in kernel space|in user space|in a VM|in a container|in a race|"
  "in a deadlock|on the data bus|in the ALU|in the register|in the pipeline|in a cache miss|"
  "in the swap file|in a stack frame|on the heap|in RAM|in ROM|in EPROM|under UV light|"
  "in a vacuum tube|in the air con|in the doorbell|on the ISS roof|in a moon buggy|past the sun|"
  "into a sunspot|in a solar flare|in deep space|in hyperspace|on Neptune|on Titan|on Europa|on Io|"
  "on Phobos";

// Objects: up to 16 characters.
const char OBJECTS[] PROGMEM =
  "a burger|the LPTs|the pizza|a pizza|a floppy|the backup|the source code|the database|the RAM|"
  "a capacitor|the last byte|the checksum|the password|the firmware|the bootloader|a diode|"
  "three resistors|the keyboard|the spacebar|the mouse|the moon|a comet|an asteroid|the rover|"
  "the satellite|a cable|the USB cable|the power cord|the fuse|the heatsink|the fan|the CPU|"
  "the logs|the error log|the stack trace|a semicolon|the coffee|a cake|a donut|the manual|"
  "the deadline|the budget|the roadmap|the spec|the docs|the unit tests|the test suite|the release|"
  "the hotfix|the patch|the update|the OS|the kernel|a driver|the printer|a paper jam|the toner|"
  "the ink|a cassette|the tape|punch cards|a vacuum tube|a transistor|an electron|a photon|"
  "a neutrino|a quark|a qubit|the Wi-Fi|the antenna|the dish|rocket fuel|the launch pad|"
  "a spacesuit|a helmet|the airlock|the solar panel|the sun|a black hole|Mars|Saturn's rings|"
  "the motherboard|the jumper|a sticky note|the cursor|the mouse wheel|the reset button|"
  "a USB stick|the dongle|the serial port|a ribbon cable|a SCSI cable|the terminator|a modem|"
  "a dial tone|the handshake|the parity bit|the stop bit|the start bit|the baud rate|the buffer|"
  "the stack|the heap|a pointer|a null pointer|a memory leak|a bug report|a ticket|the inbox|"
  "an email|the spam|a cookie|the cookies|a byte|a nibble|a bit|a pixel|the screensaver|"
  "the wallpaper|the desktop|the trash can|the clipboard|the undo button|a backup tape|"
  "the mainframe|a punch card|the teletype|a dot matrix|the plotter pen|a soldering iron|"
  "the solder|flux|a breadboard|an LED|the LEDs|a relay|the fuse box|a battery|the batteries|"
  "a transformer|a coil|a magnet|a magnetron|an oscilloscope|a multimeter|the probe|a rocket|"
  "a meteor|stardust|moon dust|a moon rock|a space rock|an alien egg|alien goo|a UFO|"
  "a flying saucer|a wormhole|a nebula|the Milky Way|the galaxy|the universe|the Big Bang|gravity|"
  "the vacuum|a lunch box|a sandwich|cold pizza|cold coffee|an energy drink|instant noodles|"
  "a kebab|a hot dog|a taco|the snacks|the last donut|the last cookie|a sock|the socks|the chair|"
  "the desk|the office plant|a cactus|the fridge|the microwave|the toaster|the coffee maker|"
  "the elevator|the stairs|the fire alarm|the sprinklers|the boss's mug|the boss's car|"
  "the boss's chair|the meeting room|the whiteboard|a marker|the stapler|a paperclip|"
  "the fax machine|the shredder|the BIOS|the compiler|the cache|an interrupt|a bug|a virus|a worm|"
  "a trojan|a deadlock|a segfault|the firewall|the cloud|a robot|the AI|a chatbot|the algorithm|"
  "the spacecraft|the space probe|the lander|the shuttle|Jupiter|Saturn|Pluto|a quasar|a pulsar|"
  "a supernova|the oscilloscope|the multimeter|the Arduino|the EPROM|the ROM|a lonely byte|"
  "a lost bit|a dead pixel|the Enter key|the radar|the laser|a resistor|an inductor|a fuse|"
  "a thermistor|a solder joint|the hotspot|the spam filter|the blue screen|the sprint|the meeting|"
  "the webinar|the scrum board|the Gantt chart|the org chart|the budget plan|the invoice|"
  "the receipt|the warranty|the license key|the product key|a floppy disk|a zip drive|a tape reel|"
  "a ROM chip|a RAM chip|a CPU socket|a heat sink|a cooling fan|a power brick|an AC adapter|"
  "a car battery|a 9V battery|AA batteries|a coin cell|a light bulb|a neon lamp|a nixie tube|"
  "a cathode|an anode|a grid|a filament|a crystal ball|a tin can|a toaster oven|a waffle iron|"
  "a frying pan|a teapot|a teacup|a coffee mug|a soup ladle|a whisk|a rubber duck|a plastic fork|"
  "a paper plate|a pizza box|a pizza slice|a cold burger|a stale donut|a moldy sandwich|"
  "a sad salad|a soggy taco|a melted candle|a broken mirror|a cracked screen|a dead battery|"
  "a burnt fuse|a lost cable|a tangled cable|a bent pin|a loose wire|a cold solder|a blown fuse|"
  "a leaky battery|a rusty screw|a stripped screw|a missing screw|three screws|the last screw|"
  "a spare part|a spare key|the master key|the admin rights|the sudo rights|a firewall rule|"
  "the error code|error 404|error 418|a race condition|an infinite loop|a dead loop|a goto|"
  "a spaghetti code|legacy code|the old code|COBOL code|the comments|the variables|the constants|"
  "a magic number|the random seed|the entropy|the time zone|the leap second|the year 2038|"
  "the Y2K bug|a timestamp|the uptime|the downtime|the outage|the incident|the post-mortem|"
  "the hotline|the helpdesk|a support ticket|the FAQ|the README|the changelog|the license|"
  "the patent|the trademark|the moon landing|the countdown|the orbit|the trajectory|the payload|"
  "the booster|the nose cone|the heat shield|the parachute|the escape pod|the space toilet|"
  "a space burrito|a space potato|Mars soil|lunar dust|a meteorite|a comet tail|the Kuiper belt|"
  "the Oort cloud|a red dwarf|a white dwarf|a neutron star|a gas giant|an ice giant|a dark nebula|"
  "dark matter|dark energy|antimatter|a positron|a muon|a Higgs boson|a tachyon|a black box|"
  "the flight log|the floppy drive|the modem cable|the dial-up|the fax tone|the BIOS chip|"
  "the CMOS battery|the jumper cap|the DIP switch|the IRQ|the DMA channel|the COM port|"
  "the LPT port|a parallel cable|a null modem|a coax cable|a hard disk|a disk platter|a read head|"
  "the spindle|a bad sector|the boot sector|the registry|the swap file|a core dump|a crash dump|"
  "a kernel module|a device driver|the mouse driver|a stack frame|a heap block|the address bus|"
  "the accumulator|the zero flag|the carry flag|an opcode|a NOP|the reset vector|a quartz crystal|"
  "a 555 timer|an op-amp|a zener diode|an LED strip|a 7-segment LED|a relay|a motor|"
  "a servo|a DC motor|a solar cell|a Geiger counter|Tesla|a spark gap|a lightning rod|"
  "the ground wire|the live wire|a power strip|the main fuse|the power grid|the server rack|"
  "the patch panel|the KVM switch|a blade server|a tape robot|the air con|a space heater|"
  "a hair dryer|a vacuum cleaner|a robot vacuum|a lawn mower|a smart bulb|a smart plug|"
  "a smart lock|the thermostat|Jesus|a lunar module|a moon buggy|a space glove|a space wrench|"
  "a lost wrench|a space sock|a star chart|a light-year|a parsec|a radio signal|a pulsar beep|"
  "an alien text|an alien selfie|alien spam|a crop circle|a tin foil hat|a rocket nozzle|"
  "a fuel tank|liquid oxygen|a sunspot|a solar flare|the solar wind|a cosmic ray|the ozone layer|"
  "Stratosphere|a sad pancake|a cursed pie|a glowing bagel|a haunted muffin|a weeping onion|"
  "a lonely pickle|a burnt toast|a frozen pizza|a soggy waffle|a stale pretzel|an angry lemon|"
  "a moody potato|a nervous cheese|a gloomy bagel";

// first character of item idx of a list
const char *itemAt(const char *list, uint16_t idx) {
  while (idx) {
    if (pgm_read_byte(list++) == '|') idx--;
  }
  return list;
}

// number of items of a list
uint16_t countItems(const char *list) {
  uint16_t n = 1;
  char c;
  while ((c = pgm_read_byte(list++)))
    if (c == '|') n++;
  return n;
}

// copy field f (fields separated by ',') of the item at p into out; returns its length
uint8_t copyField(const char *p, uint8_t f, char *out) {
  while (f) {
    if (pgm_read_byte(p++) == ',') f--;
  }
  uint8_t n = 0;
  char c;
  while ((c = pgm_read_byte(p++)) && c != '|' && c != ',') out[n++] = c;
  out[n] = 0;
  return n;
}

// third person singular of a verb phrase (in RAM): -s, -es or -ies on its first word
uint8_t thirdPerson(const char *base, char *out) {
  uint8_t w = 0;
  while (base[w] && base[w] != ' ') w++;
  memcpy(out, base, w);
  char a = base[w - 1], b = w > 1 ? base[w - 2] : 'a';
  uint8_t n = w;
  if (a == 'y' && !strchr("aeiou", b)) {
    out[n - 1] = 'i';
    out[n++] = 'e';
    out[n++] = 's';
  } else if (a == 's' || a == 'x' || a == 'z' || a == 'o' || (a == 'h' && (b == 'c' || b == 's'))) {
    out[n++] = 'e';
    out[n++] = 's';
  } else {
    out[n++] = 's';
  }
  strcpy(out + n, base + w);
  return n + strlen(base + w);
}

// the verb item v in the tense, for a singular or plural subject; returns the length
uint8_t verbForm(const char *v, Tense t, bool plural, char *out) {
  if (t == PAST) return copyField(v, 1, out);
  if (t == FUTURE) {
    strcpy(out, "will ");
    return 5 + copyField(v, 0, out + 5);
  }
  if (plural) return copyField(v, 0, out);
  char base[24];
  copyField(v, 0, base);
  return thirdPerson(base, out);
}

// a random item of a list with at most room characters into out; returns its
// length, 0 if none fits
uint8_t pickFit(const char *list, int8_t room, char *out) {
  if (room <= 0) return 0;
  uint16_t fit = 0, len = 0;
  const char *p = list;
  char c;
  do {                                        // pass 1: count the items that fit
    c = pgm_read_byte(p++);
    if (c == '|' || !c) {
      if (len <= (uint16_t)room) fit++;
      len = 0;
    } else {
      len++;
    }
  } while (c);
  if (!fit) return 0;
  uint16_t k = random(fit);
  const char *start = p = list;
  do {                                        // pass 2: copy the k-th of them
    c = pgm_read_byte(p++);
    if (c == '|' || !c) {
      if (len <= (uint16_t)room && k-- == 0) return copyField(start, 0, out);
      start = p;
      len = 0;
    } else {
      len++;
    }
  } while (c);
  return 0;
}

// which day the question asks for (upper or lower case)
Tense tenseOf(const char *q) {
  char u[48];
  uint8_t n = 0;
  for (; q[n] && n < sizeof(u) - 1; n++) u[n] = (q[n] >= 'a' && q[n] <= 'z') ? q[n] - 32 : q[n];
  u[n] = 0;
  if (strstr(u, "YESTERDAY")) return PAST;
  if (strstr(u, "TOMORROW")) return FUTURE;
  if (strstr(u, "TODAY")) return PRESENT;
  return UNKNOWN;
}

// a sentence for the tense, at most LINE_LEN characters, into out
// (LINE_LEN + 1 bytes); returns its length
uint8_t makeQuote(Tense t, char *out) {
  static uint16_t ns, nt, ni;
  if (!ns) {
    ns = countItems(SUBJECTS);
    nt = countItems(VERBS_T);
    ni = countItems(VERBS_I);
  }
  if (t == UNKNOWN) {
    strcpy(out, "Ask about today, please.");
    return strlen(out);
  }
  char subj[24], verb[24], buf[64];
  for (;;) {
    // pattern: 0 = object (40 %), 1 = place (25 %), 2 = adverb (20 %), 3 = adverb first (15 %)
    uint8_t r = random(100);
    uint8_t pat = r < 40 ? 0 : r < 65 ? 1 : r < 85 ? 2 : 3;
    uint8_t sl = copyField(itemAt(SUBJECTS, random(ns)), 0, subj);
    bool plural = subj[0] == '+';
    if (plural) memmove(subj, subj + 1, sl--);
    uint8_t vl = pat == 0 ? verbForm(itemAt(VERBS_T, random(nt)), t, plural, verb)
                          : verbForm(itemAt(VERBS_I, random(ni)), t, plural, verb);
    uint8_t n;
    if (pat == 3) {                           // "Gloomily the CPU died."
      n = pickFit(ADVERBS, (int8_t)(LINE_LEN - 3 - sl - vl), buf);
      if (!n) continue;
      buf[0] = toupper(buf[0]);
      buf[n++] = ' ';
      memcpy(buf + n, subj, sl);
      n += sl;
      buf[n++] = ' ';
      memcpy(buf + n, verb, vl);
      n += vl;
    } else {                                  // "The CPU died gloomily."
      memcpy(buf, subj, sl);
      buf[0] = toupper(buf[0]);
      n = sl;
      buf[n++] = ' ';
      memcpy(buf + n, verb, vl);
      n += vl;
      buf[n++] = ' ';
      const char *tail = pat == 0 ? OBJECTS : pat == 1 ? PLACES : ADVERBS;
      uint8_t tl = pickFit(tail, (int8_t)(LINE_LEN - 1 - n), buf + n);
      if (!tl) continue;                      // too long: try again
      n += tl;
    }
    buf[n++] = '.';
    buf[n] = 0;
    strcpy(out, buf);
    return n;
  }
}
// ==== quote generator end

SoftwareSerial pc(A5, A4);              // RX from PC7, TX to PB2
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);    // LCD Keypad Shield (optional)

char question[48];
uint8_t qn;
uint32_t lastByte;
bool seeded;

void answer() {
  question[qn] = 0;
  if (qn < 3) {                         // noise (e.g. PC7 switched on): no question
    qn = 0;
    return;
  }
  if (!seeded) {                        // the moment of the first question is random
    randomSeed(micros() ^ ((uint32_t)analogRead(A1) << 16));
    seeded = true;
  }
  Tense t = tenseOf(question);
  char quote[LINE_LEN + 1];
  makeQuote(t, quote);
  delay(ANSWER_DELAY);                  // the PC-1500 is now waiting in CALL SI
  pc.print(quote);
  for (uint8_t i = strlen(quote); i < LINE_LEN; i++) pc.write(' ');   // always LINE_LEN + CR
  pc.write('\r');
  Serial.print(F("Q: "));
  Serial.print(question);
  Serial.print(F("   A: "));
  Serial.println(quote);
  lcd.clear();
  lcd.print(t == PRESENT ? F("Today:") : t == FUTURE ? F("Tomorrow:") : t == PAST ? F("Yesterday:") : F("?"));
  lcd.setCursor(0, 1);
  lcd.print(quote);                     // the first 16 characters
  qn = 0;
}

void setup() {
  Serial.begin(115200);
  pc.begin(BAUD);                       // SoftwareSerial: TX idles at mark (high)
  lcd.begin(16, 2);
  lcd.print(F("PC-1500 Gloom"));
  lcd.setCursor(0, 1);
  lcd.print(BAUD);
  lcd.print(F(" bps"));
  Serial.print(F("PC-1500 gloomy quote of the day, "));
  Serial.print(BAUD);
  Serial.print(F(" bps. Words: "));
  Serial.print(countItems(SUBJECTS));
  Serial.print(F(" subjects, "));
  Serial.print(countItems(VERBS_T) + countItems(VERBS_I));
  Serial.print(F(" verbs, "));
  Serial.print(countItems(OBJECTS));
  Serial.print(F(" objects, "));
  Serial.print(countItems(PLACES));
  Serial.print(F(" places, "));
  Serial.print(countItems(ADVERBS));
  Serial.println(F(" adverbs."));
  Serial.println(F("Waiting for the PC-1500..."));
}

void loop() {
  while (pc.available()) {
    char c = pc.read();
    lastByte = millis();
    if (c == '\r' || c == '\n') {
      answer();
    } else if (qn < sizeof(question) - 1) {
      question[qn++] = c;
    }
  }
  if (qn && millis() - lastByte > 100) answer();   // no CR: the question ends after 100 ms
}
