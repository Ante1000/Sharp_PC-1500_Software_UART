/*
  UART_Quote.ino - "quote of the day" for the Sharp PC-1500(A) with SERINOUT v7.2
  (Software UART), 19200 bps, Arduino UNO. The LCD Keypad Shield is optional.

  The PC-1500 program pc1500_uart_quote-v1.0.txt asks
      What's for today?   /   What's for tomorrow?   /   What's for yesterday?
  (ended with CR). The UNO builds a short sentence from the word lists below,
      today:      <subject> <verb, present> <object>.      Luck finds a golden key.
      tomorrow:   <subject> will <verb> <object>.           A cat will fix the bus.
      yesterday:  <subject> <verb, past> <object>.          The moon stole hot tea.
  at most 26 characters (one line of the PC-1500 display), and sends it back,
  followed by CR.

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
#define ANSWER_DELAY 200 // ms: the PC-1500 goes from CALL SO to CALL SI meanwhile

enum Tense : uint8_t { PRESENT, FUTURE, PAST, UNKNOWN };

// ==== quote generator begin (no hardware access; the same code is compiled
// ==== on a PC for tests)

// Word lists: items separated by '|'. Add or change words as you like; a
// sentence longer than LINE_LEN is never sent (another one is chosen).
// Subjects: singular (the verb gets -s), first letter capital.
const char SUBJECTS[] PROGMEM =
  "A cat|A dog|A duck|A goat|A spy|A ghost|A robot|A pirate|A wizard|A poet|"
  "A clown|A monkey|A penguin|A pigeon|A stranger|The moon|The sun|The wind|"
  "The rain|The mayor|The baker|The postman|The printer|Your boss|Your cat|"
  "Your phone|Your sister|Your PC-1500|Grandma|Uncle Bob|Luck|Love|Fate|Coffee";

// Verbs: base form, third person singular, past tense.
const char VERBS[] PROGMEM =
  "find,finds,found|bring,brings,brought|eat,eats,ate|steal,steals,stole|"
  "fix,fixes,fixed|lose,loses,lost|paint,paints,painted|hide,hides,hid|"
  "break,breaks,broke|buy,buys,bought|cook,cooks,cooked|hug,hugs,hugged|"
  "chase,chases,chased|forget,forgets,forgot|win,wins,won|send,sends,sent|"
  "meet,meets,met|borrow,borrows,borrowed|open,opens,opened|sell,sells,sold|"
  "catch,catches,caught|drink,drinks,drank|read,reads,read|write,writes,wrote|"
  "teach,teaches,taught|follow,follows,followed|save,saves,saved|"
  "visit,visits,visited|ignore,ignores,ignored|kiss,kisses,kissed|"
  "miss,misses,missed|wash,washes,washed|taste,tastes,tasted|feed,feeds,fed|"
  "print,prints,printed|carry,carries,carried|see,sees,saw|want,wants,wanted";

// Objects: lower case.
const char OBJECTS[] PROGMEM =
  "a golden key|your lost sock|free pizza|the last cookie|a secret map|"
  "an old friend|the answer|your keys|a lucky coin|hot coffee|a bag of gold|"
  "the moon|a red balloon|good news|a paper plane|your umbrella|a new idea|"
  "the remote|a love letter|warm socks|the bus|a free lunch|your homework|"
  "a strange box|hot tea|a big smile|the wrong train|three bananas|a magic hat|"
  "a rubber duck|a floppy disk|a cassette|your password|the treasure|a cake|"
  "a bicycle|a black cat|a tiny dragon|the last pie|a song";

// number of items of a list
uint8_t countItems(const char *list) {
  uint8_t n = 1;
  for (const char *p = list; pgm_read_byte(p); p++)
    if (pgm_read_byte(p) == '|') n++;
  return n;
}

// copy field f (0 = first, fields separated by ',') of item idx into out;
// returns its length
uint8_t getItem(const char *list, uint8_t idx, uint8_t f, char *out) {
  const char *p = list;
  while (idx) {                               // skip idx items
    if (pgm_read_byte(p++) == '|') idx--;
  }
  while (f) {                                 // skip f fields
    if (pgm_read_byte(p++) == ',') f--;
  }
  uint8_t n = 0;
  char c;
  while ((c = pgm_read_byte(p)) && c != '|' && c != ',') {
    out[n++] = c;
    p++;
  }
  out[n] = 0;
  return n;
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

// a sentence for the tense, at most LINE_LEN characters; returns its length
uint8_t makeQuote(Tense t, char *out) {
  static const char HELP[] = "Ask about today, please.";
  if (t == UNKNOWN) {
    strcpy(out, HELP);
    return strlen(out);
  }
  uint8_t ns = countItems(SUBJECTS), nv = countItems(VERBS), no = countItems(OBJECTS);
  char w[24];
  for (;;) {
    uint8_t n = getItem(SUBJECTS, random(ns), 0, out);
    out[n++] = ' ';
    if (t == FUTURE) {
      strcpy(out + n, "will ");
      n += 5;
    }
    uint8_t vi = random(nv);
    n += getItem(VERBS, vi, t == PRESENT ? 1 : (t == PAST ? 2 : 0), out + n);
    out[n++] = ' ';
    // room left for the object and the full stop
    int8_t room = LINE_LEN - n - 1;
    uint8_t fit = 0;
    for (uint8_t i = 0; i < no; i++)
      if (getItem(OBJECTS, i, 0, w) <= room) fit++;
    if (!fit) continue;                       // subject + verb too long: try again
    uint8_t k = random(fit);
    for (uint8_t i = 0; i < no; i++) {
      uint8_t len = getItem(OBJECTS, i, 0, w);
      if (len <= room && k-- == 0) {
        strcpy(out + n, w);
        n += len;
        break;
      }
    }
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
