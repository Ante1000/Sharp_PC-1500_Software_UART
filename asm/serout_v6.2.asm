; v6.2: kod maszynowy identyczny z v6.1 (instalator v6.2 zmienia tylko teksty i testy w BASIC-u).
; v6.2: machine code identical to v6.1 (installer v6.2 only changes BASIC prompts and tests).
;
; =====================================================================
; SEROUT v6.1 - programowy nadajnik UART 8N1 dla Sharp PC-1500(A),
; 1200 / 2400 / 4800 / 9600 bps, polaryzacja normalna albo odwrocona
; (wybor w instalatorze). CPU LH5801 @ 1,3 MHz, wyjscie PC7 (&F008 bit 7).
;   normalna (TTL):  spoczynek i bit "1" = stan wysoki, start i "0" = niski
;   odwrocona:       spoczynek i bit "1" = stan niski,  start i "0" = wysoki
;
; Wywolanie z BASIC-a:
;   CALL SO          wyslij 1 znak z TX+0
;   CALL SO,N        wyslij N znakow z TX+0.. (1..255; 0 = 1 znak, >255 = 255)
; Zmienna N NIE jest zmieniana (powrot z C=0).
; Bufor TX: 255 bajtow od poczatku strony pamieci (TX = RAM+&300 = &4300..&43FE).
;
; Kod jak w v6.0; zapisy do PC7 to pseudo-rozkazy MARK / SPACE (FD op imm),
; ktorych kod i argument wpisuje instalator:
;   MARK  #(Y) = FD OM VM   normalnie ORI #(Y),&80, odwrocone ANI #(Y),&7F
;   SPACE #(Y) = FD OS VS   normalnie ANI #(Y),&7F, odwrocone ORI #(Y),&80
; ANI i ORI trwaja tyle samo (17 cykli), wiec czasy sie nie zmieniaja.
; Od szybkosci zaleza tylko stale petli LOP,
; ktore wpisuje instalator (zmienne BASIC-a w liniach POKE):
;   KB = n   bit danych: 79 + 11*n cykli (tablica MAME; instrukcja +3)
;   KS = n+2 bit startu (tyle samo co bit danych)
;   KT       bit stopu: ramka (start + 8 bitow + stop) >= 10,1 bitu
;   KI = n+7 spoczynek przed pierwszym znakiem (ok. 1 bit)
;   bps    1 bit [cykle]   KB   KT
;   1200      1083,3       91  106
;   2400       541,7       42   50
;   4800       270,8       17   26
;   9600       135,4        5   10   (= v55, sprawdzone na PC-1500A)
; Obie galezie (bit 0 i bit 1) maja te sama dlugosc, a zapis do portu
; nastepuje w nich w tym samym momencie (+-1 cykl).
; Licznik bitow: znacznik (sentinel) - SEC+ROR wklada 1 do bitu 7, po
; osmym SHR akumulator = 0. Z sprawdzamy przez BII A,&FF, bo wedlug
; instrukcji LH5801 rozkazy SHR/ROR nie ustawiaja flagi Z.
; Licznik znakow w UH (1..255), bez zmiennych w pamieci.
;
; Adres ladowania: SO = RAM+&0C5 (&40C5).
; Symbole wstawiane przez BASIC: TP = starszy bajt adresu bufora TX,
; KI, KS, KB, KT = stale czasowe, OM/VM/OS/VS = polaryzacja (patrz wyzej).
; Plik jest zrodlem dla tools/build_v61.py (asembler + generator POKE).
; =====================================================================
        RIE                 ; bez przerwan (timing!)
        LDA  XH
        BZS+ SMALL          ; N = 0..255
        BII  A,&80          ; CALL SO bez zmiennej: X = adres w ROM (XH>=&80)
        BZR+ N1             ;   -> 1 znak
        LDI  A,&FF          ; N = 256..32767 -> 255
        BCH+ NOK
SMALL:  LDA  XL
        BZR+ NOK            ; N = 1..255
N1:     LDI  A,1            ; N = 0 -> 1 znak
NOK:    STA  UH             ; UH = liczba znakow (1..255)
        LDI  XH,TP          ; X = bufor TX (poczatek strony)
        LDI  XL,0
        LDI  YH,&F0         ; Y = &F008 (port C)
        LDI  YL,&08
        MARK #(Y)           ; PC7 = spoczynek (mark) ...
        LDI  UL,KI          ; ... przez ~1 bit przed pierwszym startem
IDLE:   LOP  UL,IDLE
CHAR:   LIN  X              ; A = znak, X++
        SPACE #(Y)          ; bit startu (space)
        SEC
        ROR                 ; C = bit 0, znacznik 1 -> bit 7 A
        LDI  UL,KS          ; dlugosc bitu startu
SDLY:   LOP  UL,SDLY
        NOP
OUT:    BCS+ ONE            ; wyslij bit z C
        REC                 ; (wyrownanie: zapis w tym samym cyklu)
        SPACE #(Y)          ; bit 0 (space)
        BCH+ BDLY
ONE:    MARK #(Y)           ; bit 1 (mark)
        NOP                 ; (wyrownanie dlugosci galezi)
        SEC
BDLY:   LDI  UL,KB          ; dlugosc bitu danych
DLY:    LOP  UL,DLY
        REC                 ; (dostrojenie: +4 cykle)
        SHR                 ; C = nastepny bit
        BII  A,&FF          ; A = 0 -> wyslano 8 bitow
        BZR- OUT
        NOP                 ; (wyrownanie ostatniego bitu danych)
        NOP
        REC
        MARK #(Y)           ; bit stopu (mark)
        LDI  UL,KT          ; dlugosc bitu stopu (ok. 1,1 bitu)
TDLY:   LOP  UL,TDLY
        DEC  UH
        BZR- CHAR           ; nastepny znak
        REC                 ; C=0: zmienna N bez zmian
        RTN
