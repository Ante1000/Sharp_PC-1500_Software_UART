/*
  UART_Quote.ino - "quote of the day" for the Sharp PC-1500(A) with SERINOUT v7.2
  (Software UART), 19200 bps, Arduino UNO. The LCD Keypad Shield is optional.

  The PC-1500 program pc1500_uart_quote-v1.0.txt asks, in a loop and at random,
      What's for today?   /   What's for tomorrow?   /   What's for yesterday?
  (ended with CR) and shows each answer for about 8 s. The UNO builds a short
  sentence from the word lists below,
      today:      <subject> <verb, present> <object>.      Luck finds a golden key.
      tomorrow:   <subject> will <verb> <object>.           A cat will fix the bus.
      yesterday:  <subject> <verb, past> <object>.          The moon stole hot tea.
  at most 26 characters (one line of the PC-1500 display), and sends it back,
  filled up with spaces to 26 characters and followed by CR: always 27
  characters, so SERIN (CALL SI,M with M = 27) returns at once instead of
  waiting for its 0.5 s time-out.

  Wiring (the same as for the calibration sketch; everything at 5 V):
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

#define BAUD 19200       // the speed installed on the PC-1500 (1200 .. 19200)
#define LINE_LEN 26      // characters in one line of the PC-1500 display
#define ANSWER_DELAY 100 // ms: the PC-1500 goes from CALL SO to CALL SI meanwhile

enum Tense : uint8_t { PRESENT, FUTURE, PAST, UNKNOWN };

// ==== quote generator begin (no hardware access; the same code is compiled
// ==== on a PC for tests)

// Word lists in flash (486 subjects, 411 verbs, 1004 objects, about 20 KB),
// items separated by '|'. Add or change words as you like: the sketch takes
// about 28 KB of the 32 KB flash of the UNO, so about 4 KB are left - keep
// some of it free. A sentence longer than LINE_LEN is never sent, another one
// is chosen.
// Subjects: singular (the verb gets -s), first letter capital, up to 14 characters.
const char SUBJECTS[] PROGMEM =
  "A cat|A dog|A fox|A bear|A goat|A duck|An owl|A crow|A frog|A toad|A snail|A mouse|A rat|A bat|"
  "A bee|A wasp|An ant|A moth|A crab|A seal|A whale|A shark|A tiger|A lion|A zebra|A horse|A pony|"
  "A donkey|A camel|A llama|A panda|A koala|A sloth|A hippo|A rhino|A moose|A badger|A beaver|"
  "An otter|A ferret|A hamster|A parrot|A pigeon|A penguin|A turtle|A lizard|A gecko|A spider|"
  "A beetle|A squirrel|A raccoon|A hedgehog|A kitten|A puppy|A lobster|An octopus|A goose|A swan|"
  "A chicken|A rooster|A turkey|A hen|A cow|A pig|A sheep|A lamb|A wolf|A mole|A worm|A flea|"
  "A gorilla|A monkey|A chimp|A dolphin|A walrus|A flamingo|A peacock|A vulture|An eagle|A hawk|"
  "A robin|A sparrow|A canary|A goldfish|A tadpole|A bunny|A rabbit|A deer|An elk|A bison|A yak|"
  "A lemur|A mongoose|A pirate|A wizard|A witch|A knight|A king|A queen|A prince|A princess|A poet|"
  "A clown|A spy|A ghost|A robot|A stranger|A tourist|A farmer|A sailor|A baker|A butcher|A tailor|"
  "A doctor|A nurse|A dentist|A lawyer|A judge|A banker|A teacher|A student|A chef|A waiter|"
  "A barber|A plumber|A pilot|A driver|A dancer|A singer|A drummer|A painter|A sculptor|A monk|"
  "A hermit|A giant|A dwarf|An elf|A troll|A vampire|A zombie|A mummy|A ninja|A cowboy|A sheriff|"
  "A detective|A janitor|A gardener|A hacker|A gamer|A nerd|A genius|A toddler|A teenager|"
  "A grandpa|A granny|A magician|A juggler|An acrobat|A mime|A surfer|A skier|A golfer|A boxer|"
  "A jockey|A referee|A coach|A captain|A sergeant|A general|A diplomat|A mayor|A senator|A tycoon|"
  "A millionaire|A beggar|A thief|A burglar|A smuggler|A bandit|A fisherman|A hunter|A ranger|"
  "A miner|A scientist|An inventor|An astronaut|An alien|A UFO|A mermaid|A unicorn|A dragon|"
  "A phoenix|A yeti|A goblin|A fairy|A genie|A pixie|A gnome|An ogre|A centaur|A cyclops|"
  "A werewolf|A snowman|A scarecrow|A puppet|A cyborg|An android|A clone|A twin|A rival|A neighbor|"
  "A colleague|A customer|A fan|A critic|A voter|A friend|An enemy|A hero|A villain|A tenant|"
  "A rumor|A dream|A secret|A miracle|A mystery|A storm|A rainbow|A comet|A meteor|An idea|"
  "A question|A memory|A song|A poem|A letter|A parcel|A bill|A cloud|A shadow|A whisper|A breeze|"
  "A tornado|A volcano|An echo|A legend|A myth|A riddle|A rocket|A spaceship|A tractor|The moon|"
  "The sun|The wind|The rain|The snow|The fog|The sea|The river|The forest|The mayor|The baker|"
  "The postman|The printer|The king|The queen|The janitor|The landlord|The cashier|The milkman|"
  "The referee|The captain|The professor|The librarian|The barista|The butler|The gardener|"
  "The cook|The chef|The intern|The manager|The boss|The tax man|The fridge|The kettle|The toaster|"
  "The microwave|The radio|The TV|The clock|The lamp|The sofa|The universe|The internet|"
  "The algorithm|The computer|The calculator|The cassette|The robot|The parrot|The neighbor|"
  "The teacher|The doctor|The dentist|The plumber|The pilot|The pirate|The wizard|The witch|"
  "The dragon|The ghost|The butcher|The sheriff|The judge|The jury|The audience|The weather|"
  "The stars|The ocean|The mountain|The desert|The jungle|The city|The village|The government|"
  "The bank|The museum|The zoo|The circus|The orchestra|The choir|The band|The team|The family|"
  "The old man|The old lady|The little boy|Your boss|Your cat|Your dog|Your phone|Your sister|"
  "Your brother|Your mother|Your father|Your aunt|Your uncle|Your cousin|Your neighbor|"
  "Your dentist|Your doctor|Your landlord|Your teacher|Your friend|Your ex|Your twin|Your shadow|"
  "Your PC-1500|Your printer|Your laptop|Your fridge|Your car|Your bike|Your goldfish|Your parrot|"
  "Your hamster|Your granny|Your grandpa|Your luck|Your karma|Your past|Your future|Your horoscope|"
  "Your pillow|Your alarm|Your mirror|Your diary|Your wallet|Your toaster|Your kettle|Your plant|"
  "Your cactus|Your hero|Your rival|Your crush|Your coach|Your barber|Your banker|Your lawyer|"
  "Your stomach|Grandma|Grandpa|Uncle Bob|Aunt May|Santa|Mozart|Einstein|Napoleon|Cleopatra|"
  "Sherlock|Dracula|Zorro|Tarzan|Robin Hood|Shakespeare|Columbus|Picasso|Darwin|Newton|Galileo|"
  "Beethoven|Merlin|King Arthur|Cinderella|Pinocchio|Frankenstein|Hercules|Zeus|Thor|Odin|Cupid|"
  "Rudolph|Uncle Sam|Mother Nature|Father Time|Lady Luck|Mr. Nobody|Dr. Watson|Captain Hook|"
  "Peter Pan|Robinson|Don Quixote|Aladdin|Sinbad|Ulysses|Achilles|Medusa|Luck|Love|Fate|Coffee|"
  "Destiny|Karma|Time|Hope|Fortune|Chance|Magic|Science|History|Music|Gravity|Silence|Chaos|Logic|"
  "Wisdom|Fame|Money|Hunger|Sleep|Tea|Chocolate|Pizza|Soup|Cheese|Somebody|Nobody|Everybody|"
  "Someone|No one|Everyone|A tiny dragon|A lazy cat|A happy dog|A grumpy cat|An old friend|"
  "A new friend|A wise owl|A sad clown|A brave mouse|A shy ghost|A lost tourist|A rich uncle|"
  "A busy bee|A sleepy bear|A hungry wolf";

// Verbs: "base form,past tense"; also with a preposition ("look for,looked for").
// The present form is made by rule from the first word: -s, -es (s x z o ch sh)
// or -ies (consonant + y); avoid "have" and "be".
const char VERBS[] PROGMEM =
  "accept,accepted|admire,admired|adopt,adopted|adore,adored|answer,answered|arrest,arrested|"
  "avoid,avoided|bake,baked|balance,balanced|bend,bent|bite,bit|bless,blessed|boil,boiled|"
  "borrow,borrowed|break,broke|bring,brought|brush,brushed|build,built|burn,burned|bury,buried|"
  "buy,bought|call,called|carry,carried|carve,carved|catch,caught|chase,chased|check,checked|"
  "chew,chewed|choose,chose|clean,cleaned|climb,climbed|close,closed|collect,collected|"
  "color,colored|comb,combed|cook,cooked|copy,copied|count,counted|cover,covered|crack,cracked|"
  "crush,crushed|cut,cut|defend,defended|deliver,delivered|design,designed|destroy,destroyed|"
  "dig,dug|discover,discovered|dress,dressed|drink,drank|drive,drove|drop,dropped|dry,dried|"
  "eat,ate|enjoy,enjoyed|explain,explained|feed,fed|fill,filled|find,found|fix,fixed|fold,folded|"
  "follow,followed|forget,forgot|forgive,forgave|free,freed|freeze,froze|fry,fried|grab,grabbed|"
  "greet,greeted|grill,grilled|grow,grew|guard,guarded|guess,guessed|hate,hated|heat,heated|"
  "help,helped|hide,hid|hold,held|hug,hugged|hunt,hunted|ignore,ignored|invent,invented|"
  "invite,invited|iron,ironed|juggle,juggled|keep,kept|kiss,kissed|knit,knitted|lick,licked|"
  "lift,lifted|like,liked|lose,lost|love,loved|make,made|mend,mended|miss,missed|mix,mixed|"
  "move,moved|need,needed|open,opened|order,ordered|pack,packed|paint,painted|pay,paid|peel,peeled|"
  "pet,petted|pick,picked|plant,planted|play,played|polish,polished|pour,poured|praise,praised|"
  "print,printed|protect,protected|pull,pulled|push,pushed|read,read|rescue,rescued|"
  "return,returned|ride,rode|roll,rolled|ruin,ruined|save,saved|scare,scared|scratch,scratched|"
  "see,saw|sell,sold|send,sent|share,shared|shave,shaved|sign,signed|sing,sang|slice,sliced|"
  "smash,smashed|smell,smelled|sniff,sniffed|solve,solved|spill,spilled|spot,spotted|"
  "squeeze,squeezed|stamp,stamped|start,started|steal,stole|stir,stirred|stop,stopped|"
  "study,studied|swallow,swallowed|swap,swapped|take,took|taste,tasted|teach,taught|tease,teased|"
  "test,tested|tickle,tickled|touch,touched|trade,traded|train,trained|trust,trusted|tune,tuned|"
  "unlock,unlocked|use,used|visit,visited|wake,woke|want,wanted|warm,warmed|wash,washed|"
  "watch,watched|wear,wore|weigh,weighed|win,won|wrap,wrapped|write,wrote|zip,zipped|"
  "approve,approved|ask,asked|beg,begged|believe,believed|blame,blamed|blow,blew|bounce,bounced|"
  "bribe,bribed|brew,brewed|calm,calmed|capture,captured|celebrate,celebrated|charm,charmed|"
  "cheer,cheered|chop,chopped|claim,claimed|coach,coached|confuse,confused|crave,craved|"
  "cuddle,cuddled|debug,debugged|delete,deleted|doodle,doodled|drag,dragged|draw,drew|dust,dusted|"
  "earn,earned|email,emailed|envy,envied|examine,examined|expect,expected|feel,felt|flip,flipped|"
  "frame,framed|glue,glued|grade,graded|hack,hacked|hammer,hammered|handle,handled|hang,hung|"
  "hear,heard|heal,healed|hire,hired|hum,hummed|imagine,imagined|inherit,inherited|"
  "inspect,inspected|label,labeled|lead,led|lend,lent|light,lit|load,loaded|lock,locked|"
  "mail,mailed|marry,married|measure,measured|melt,melted|milk,milked|mop,mopped|name,named|"
  "nibble,nibbled|notice,noticed|own,owned|park,parked|pat,patted|patch,patched|pinch,pinched|"
  "plan,planned|poke,poked|post,posted|predict,predicted|prepare,prepared|program,programmed|"
  "promise,promised|puzzle,puzzled|quote,quoted|rate,rated|recycle,recycled|remember,remembered|"
  "rent,rented|repair,repaired|replace,replaced|reset,reset|rewind,rewound|rip,ripped|"
  "roast,roasted|rule,ruled|salt,salted|scan,scanned|scrub,scrubbed|search,searched|sew,sewed|"
  "shrink,shrank|sketch,sketched|skip,skipped|snatch,snatched|soak,soaked|sort,sorted|spend,spent|"
  "spin,spun|split,split|spoil,spoiled|spray,sprayed|squash,squashed|stack,stacked|steer,steered|"
  "stretch,stretched|stroke,stroked|stuff,stuffed|support,supported|surprise,surprised|sweep,swept|"
  "tame,tamed|tape,taped|text,texted|thank,thanked|tie,tied|toast,toasted|tow,towed|track,tracked|"
  "trap,trapped|trick,tricked|trim,trimmed|type,typed|unpack,unpacked|unwrap,unwrapped|"
  "update,updated|upload,uploaded|vacuum,vacuumed|wax,waxed|welcome,welcomed|whip,whipped|"
  "wipe,wiped|wreck,wrecked|get,got|give,gave|do,did|lasso,lassoed|tag,tagged|rob,robbed|"
  "hypnotize,hypnotized|wait for,waited for|ask for,asked for|pay for,paid for|care for,cared for|"
  "search for,searched for|vote for,voted for|hope for,hoped for|talk to,talked to|sing to,sang to|"
  "write to,wrote to|point at,pointed at|stare at,stared at|shout at,shouted at|bark at,barked at|"
  "laugh at,laughed at|wink at,winked at|smile at,smiled at|trip over,tripped over|sit on,sat on|"
  "step on,stepped on|dream of,dreamed of|dream about,dreamed about|worry about,worried about|"
  "think of,thought of|run from,ran from|hide from,hid from|argue with,argued with|"
  "play with,played with|flirt with,flirted with|fall for,fell for|look for,looked for|"
  "listen to,listened to|dance with,danced with|knock on,knocked on|jump over,jumped over|"
  "sneeze on,sneezed on|lean on,leaned on|count on,counted on|rely on,relied on|spy on,spied on|"
  "chat with,chatted with|fight for,fought for|sleep on,slept on|feast on,feasted on|"
  "snack on,snacked on|bet on,bet on|ban,banned|boost,boosted|browse,browsed|bump into,bumped into|"
  "cancel,canceled|click,clicked|cherish,cherished|chill,chilled|clone,cloned|compose,composed|"
  "conquer,conquered|crown,crowned|dodge,dodged|erase,erased|fetch,fetched|film,filmed|"
  "flatter,flattered|format,formatted|gather,gathered|guide,guided|hatch,hatched|hoard,hoarded|"
  "interview,interviewed|knead,kneaded|nudge,nudged|pamper,pampered|paste,pasted|pickle,pickled|"
  "pocket,pocketed|press,pressed|reboot,rebooted|sample,sampled|scold,scolded|seal,sealed|"
  "serve,served|shuffle,shuffled|sip,sipped|slurp,slurped|smuggle,smuggled|snap,snapped|"
  "sprinkle,sprinkled|steam,steamed|strum,strummed|summon,summoned|swipe,swiped|toss,tossed|"
  "treasure,treasured|unplug,unplugged|upgrade,upgraded|whistle at,whistled at|zap,zapped";

// Objects: lower case, up to 16 characters.
const char OBJECTS[] PROGMEM =
  "a golden key|your lost sock|free pizza|the last cookie|a secret map|an old friend|the answer|"
  "your keys|a lucky coin|hot coffee|a bag of gold|the moon|a red balloon|good news|a paper plane|"
  "your umbrella|a new idea|the remote|a love letter|warm socks|the bus|a free lunch|your homework|"
  "a strange box|hot tea|a big smile|the wrong train|three bananas|a magic hat|a rubber duck|"
  "a floppy disk|a cassette|your password|the treasure|a cake|a bicycle|a black cat|a tiny dragon|"
  "the last pie|a song|a pie|cold tea|a donut|a bagel|a muffin|a cookie|a pizza|cold pizza|"
  "a burger|a hot dog|a taco|a burrito|sushi|a banana|an apple|a lemon|a melon|a pear|a peach|"
  "a plum|a cherry|a grape|a carrot|a potato|an onion|garlic|a cucumber|a pickle|cheese|"
  "blue cheese|a sandwich|a pancake|a waffle|an omelet|an egg|ten eggs|soup|hot soup|cold soup|"
  "noodles|spaghetti|rice|popcorn|chocolate|dark chocolate|ice cream|a lollipop|candy|jelly beans|"
  "a cupcake|honey|jam|butter|bread|a baguette|a pretzel|coffee|cold coffee|espresso|lemonade|"
  "orange juice|milk|warm milk|a smoothie|a milkshake|cocoa|cake crumbs|the leftovers|a lunch box|"
  "a picnic|breakfast|lunch|dinner|a snack|a feast|a sock|a shoe|your slippers|a hat|a top hat|"
  "a scarf|a glove|a mitten|an umbrella|a raincoat|a towel|a pillow|a blanket|a teapot|a kettle|"
  "a spoon|a fork|a cup|a mug|a plate|a bowl|a broom|a mop|a bucket|a ladder|a candle|a lamp|"
  "a mirror|a clock|an alarm clock|a key|a rusty key|a lock|a door|a window|a chair|a sofa|a table|"
  "a bed|a carpet|a vase|flowers|a rose|a tulip|a cactus|a plant|a toaster|a fridge|the fridge|"
  "a radio|the TV|a phone|your phone|a charger|a battery|batteries|a light bulb|a fuse|a PC-1500|"
  "a calculator|a printer|a modem|a joystick|a mouse|a keyboard|a laptop|a robot|a chip|a cable|"
  "a USB stick|a password|a bug|the bug|a byte|a pixel|an emoji|an email|a text|a selfie|a photo|"
  "a video|a playlist|a record|a vinyl|a CD|a game|a puzzle|a crossword|a sudoku|a manual|a recipe|"
  "a map|a treasure map|a compass|a telescope|a camera|a star|a comet|a cloud|a rainbow|a storm|"
  "the rain|snow|a snowman|a snowball|a leaf|a tree|an acorn|a pine cone|a mushroom|a stone|"
  "a pebble|a rock|a shell|a feather|a nest|the cat|a dog|a duck|a goldfish|a frog|a snail|"
  "a turtle|a parrot|a pony|a unicorn|a dragon|bad news|a big idea|the truth|a secret|a joke|"
  "a riddle|a rumor|a promise|a wish|a dream|a plan|a clue|a hint|a chance|luck|a miracle|"
  "a surprise|a hug|a kiss|a compliment|an apology|a medal|a prize|a trophy|a ticket|a free ticket|"
  "a coupon|a discount|a bargain|a refund|a bonus|a raise|a fortune|a gold coin|a coin|a dollar|"
  "a euro|a penny|a wallet|a purse|a receipt|a bill|the bill|a parcel|a letter|a postcard|a note|"
  "a poem|a story|a book|a novel|a comic|a diary|a newspaper|a magazine|a pencil|a pen|a crayon|"
  "a brush|a paintbrush|a ruler|an eraser|glue|scissors|paper|a kite|a balloon|a ball|"
  "a ball of yarn|a yo-yo|a teddy bear|a doll|a toy car|a toy train|a train|the last train|"
  "the last bus|a taxi|a scooter|a car|a boat|a canoe|a rocket|a spaceship|a UFO|a plane|"
  "a red apple|a green apple|a lost key|a lost dog|a lost cat|a lost wallet|a lost glove|"
  "an old shoe|an old map|an old coin|an old radio|an old clock|an old song|an old photo|"
  "an old letter|a new hat|a new car|a new phone|a new job|a new friend|a new house|a new song|"
  "a big cake|a big box|a big fish|a small fish|a tiny fish|a tiny mouse|a tiny key|a tiny hat|"
  "a huge cake|a huge rock|a golden egg|a golden ring|a silver spoon|a silver coin|a crystal ball|"
  "a glass slipper|a wooden spoon|a wooden horse|a paper hat|a paper crown|a crown|a ring|"
  "a necklace|a bracelet|a watch|a pocket watch|a diamond|a ruby|a pearl|an emerald|a sapphire|"
  "a jewel|the jewels|the crown jewels|a treasure|a treasure chest|a chest|a box|a gift|a present|"
  "a gift card|a free hug|a high five|a wink|a wave|a smile|a frown|a sigh|a sneeze|a hiccup|"
  "a yawn|a nap|a long nap|a holiday|a vacation|a weekend|a day off|a rainy day|a sunny day|"
  "a quiet hour|a long walk|a short walk|a bath|a hot bath|a shower|a haircut|a beard|a mustache|"
  "a wig|a toupee|a hairbrush|a comb|a toothbrush|toothpaste|soap|shampoo|a sponge|a bubble|"
  "bubbles|a balloon dog|a magic wand|a wand|a broomstick|a spell|a potion|a fairy tale|a legend|"
  "a myth|a ghost story|a mystery|a fingerprint|a footprint|a shadow|a whisper|a scream|a lullaby|"
  "a symphony|a guitar|a violin|a piano|a drum|a trumpet|a flute|a harmonica|a banjo|a tuba|a bell|"
  "a whistle|a horn|a microphone|an amplifier|a speaker|a radio show|a podcast|a movie|a film|"
  "a cartoon|a sitcom|a soap opera|a quiz|a contest|a race|a marathon|a match|a goal|a penalty|"
  "a gold medal|a silver medal|a bronze medal|a red card|a yellow card|a tennis ball|a football|"
  "a basketball|a baseball|a golf ball|a bowling ball|a skateboard|a surfboard|a snowboard|a sled|"
  "skis|ice skates|a helmet|a tent|a sleeping bag|a campfire|a marshmallow|sausages|a grill|"
  "a barbecue|a frying pan|a saucepan|a pot|an oven|a stove|a microwave|a blender|a mixer|a whisk|"
  "a rolling pin|a cookbook|a menu|a tip|a table for two|a reservation|an alien|a phoenix|a yeti|"
  "a goblin|a fairy|a genie|a gnome|an ogre|a werewolf|a scarecrow|a puppet|your shadow|"
  "your printer|your laptop|your car|your bike|your pillow|your alarm|your mirror|your diary|"
  "your wallet|your toaster|your kettle|your plant|your cactus|your socks|your shoes|your hat|"
  "your scarf|your gloves|your coat|your jacket|your shirt|your tie|your watch|your ring|"
  "your glasses|your bag|your suitcase|your passport|your ticket|your lunch|your dinner|"
  "your breakfast|your coffee|your tea|your cake|your cookies|your sandwich|your pizza|your soup|"
  "your fries|your salad|your dessert|your snack|your money|your savings|your luck|your future|"
  "your past|your secret|your dream|your idea|your plan|your poem|your song|your photo|your selfie|"
  "your email|your notes|your book|your pen|your pencil|your desk|your chair|your bed|your sofa|"
  "your garden|your roses|your lawn|your fence|your garage|your attic|your basement|your kitchen|"
  "your fridge|your oven|your TV|your remote|your radio|your clock|a cat|a rat|a bat|a bee|an ant|"
  "a moth|a crab|a seal|a whale|a shark|a tiger|a lion|a zebra|a horse|a donkey|a camel|a llama|"
  "a panda|a koala|a sloth|a hippo|a rhino|a moose|a badger|a beaver|an otter|a ferret|a pigeon|"
  "a penguin|a lizard|a gecko|a spider|a beetle|a squirrel|a raccoon|a hedgehog|a lobster|"
  "an octopus|a goose|a swan|a chicken|a rooster|a turkey|a hen|a cow|a pig|a sheep|a lamb|a wolf|"
  "a mole|a worm|a flea|a gorilla|a monkey|a dolphin|a walrus|a flamingo|a peacock|a vulture|"
  "an eagle|a hawk|a robin|a sparrow|a canary|a tadpole|a deer|a bison|a yak|a lemur|a toad|a fox|"
  "a bear|a goat|an owl|a crow|the sun|the stars|the sea|the ocean|the river|the lake|the pond|"
  "the beach|the island|the forest|the jungle|the desert|the mountain|the hill|the valley|the cave|"
  "the castle|the tower|the bridge|the tunnel|the road|the street|the park|the garden|the zoo|"
  "the circus|the museum|the library|the school|the office|the bank|the shop|the market|"
  "the station|the airport|the harbor|the lighthouse|the windmill|the barn|the farm|the village|"
  "the city|the capital|the world|the universe|the galaxy|the planet|the sky|the wind|the snow|"
  "the fog|the clouds|the thunder|the lightning|the weather|the news|the internet|the computer|"
  "the keyboard|the screen|the cursor|the program|the code|the password|the manual|"
  "the instructions|the rules|the law|the contract|the deal|the plan|the map|the key|the door|"
  "the gate|the window|the stairs|the elevator|the roof|the chimney|the attic|the basement|"
  "the garage|the kitchen|the bathroom|the bedroom|the sofa|the carpet|the curtains|the lamp|"
  "the clock|the mirror|the piano|the guitar|the drums|the radio|the record|the song|the poem|"
  "the book|the letter|the parcel|the receipt|the invoice|the tax|the rent|the change|the coins|"
  "the money|the cash|the gold|the silver|the diamonds|the pearls|the crown|the throne|the dragon|"
  "the ghost|the ship|the boat|the anchor|the sail|the last word|the last laugh|the first prize|"
  "the jackpot|the lottery|the trophy|the medal|the cup|the ball|the goal|the game|the score|"
  "the race|the finish line|the starting gun|the stage|the spotlight|the curtain|the show|"
  "the movie|the popcorn|the ticket|the seat|the end|a happy end|a fresh start|a second chance|"
  "a lucky break|a good deal|a bad deal|a great idea|a bad idea|a crazy idea|a silly hat|"
  "a funny joke|a bad joke|an old joke|a long story|a short story|a true story|a sad song|"
  "a happy song|a love song|a pop song|a rock song|a jazz tune|a waltz|a tango|a disco ball|"
  "a dance floor|a party|a birthday party|a surprise party|a birthday cake|a wedding cake|"
  "a cream pie|an apple pie|a cherry pie|a pumpkin pie|a pumpkin|a pineapple|a coconut|a mango|"
  "a kiwi|a papaya|an avocado|a tomato|a pepper|a chili|a bean|beans|peas|corn|a radish|a turnip|"
  "a beet|a cabbage|a lettuce|spinach|broccoli|a cauliflower|a zucchini|an eggplant|a pumpkin seed|"
  "nuts|a walnut|a peanut|an almond|a hazelnut|a chestnut|a raisin|a fig|a date|an olive|olive oil|"
  "vinegar|salt|pepper|sugar|flour|yeast|a lemon tart|a fruit salad|a salad|a stew|a curry|a kebab|"
  "a pasta dish|lasagna|a pizza slice|a cheese board|a fondue|a sundae|a soda|a cola|a juice|"
  "a cup of tea|a cup of coffee|a mug of cocoa|a glass of milk|a bottle|a jar|a can|a tin|"
  "a box of tea|a teabag|a sugar cube|a biscuit|a cracker|a croissant|a scone|a brownie|a toffee|"
  "a caramel|a gummy bear|a candy bar|a cookie jar|a cake tin|a lunchbox|a lava lamp|a jukebox|"
  "a pinball|a typewriter|a telegram|a stamp|a snow globe|a music box|a jigsaw|a chess set|a pawn|"
  "two dice|a card|an ace|a joker|a deck of cards|a magic trick|a marble|a sticker|a badge|"
  "a button|a zipper|a sock puppet|a puppet show|a fan letter|an autograph|a love poem|a haiku|"
  "a limerick|a sonnet|a cheat code|a high score|an extra life|a sprite|a cursor|a beep|"
  "a reset button|a tape deck|a mixtape|a transistor|a breadboard|a drone|a satellite|a meteorite|"
  "a moon rock|a space suit|a sunflower|a daisy|an orchid|a bonsai|a clover|a ladybug|a butterfly|"
  "a firefly|a starfish|a seahorse|a snowflake|an icicle|a puddle|a sandcastle|a seashell";

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

// a random object of at most room characters into out; returns its length, 0 if none fits
uint8_t pickObject(int8_t room, char *out) {
  if (room <= 0) return 0;
  uint16_t fit = 0, len = 0;
  const char *p = OBJECTS;
  char c;
  do {                                        // pass 1: count the objects that fit
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
  const char *start = p = OBJECTS;
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
  static uint16_t ns, nv;
  if (!ns) {
    ns = countItems(SUBJECTS);
    nv = countItems(VERBS);
  }
  if (t == UNKNOWN) {
    strcpy(out, "Ask about today, please.");
    return strlen(out);
  }
  char buf[48], verb[24], obj[24];
  for (;;) {
    uint8_t n = copyField(itemAt(SUBJECTS, random(ns)), 0, buf);
    buf[n++] = ' ';
    const char *v = itemAt(VERBS, random(nv));
    if (t == PAST) {
      n += copyField(v, 1, buf + n);
    } else if (t == FUTURE) {
      strcpy(buf + n, "will ");
      n += 5;
      n += copyField(v, 0, buf + n);
    } else {
      copyField(v, 0, verb);
      n += thirdPerson(verb, buf + n);
    }
    buf[n++] = ' ';
    uint8_t len = pickObject((int8_t)(LINE_LEN - 1 - n), obj);   // room for the object and '.'
    if (!len) continue;                       // subject + verb too long: try again
    memcpy(out, buf, n);
    memcpy(out + n, obj, len);
    n += len;
    out[n++] = '.';
    out[n] = 0;
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
  lcd.print(F("PC-1500 Quote"));
  lcd.setCursor(0, 1);
  lcd.print(BAUD);
  lcd.print(F(" bps"));
  Serial.print(F("PC-1500 quote of the day, "));
  Serial.print(BAUD);
  Serial.print(F(" bps. Words: "));
  Serial.print(countItems(SUBJECTS));
  Serial.print(F(" subjects, "));
  Serial.print(countItems(VERBS));
  Serial.print(F(" verbs, "));
  Serial.print(countItems(OBJECTS));
  Serial.println(F(" objects."));
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
