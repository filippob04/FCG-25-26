# Relazione Progetto - Baldini Filippo S6393212

Relazione del progetto di Fondamnti di Computer Grafica. \
Versionamento a tappe dello sviluppo di un semplice simulatore di volo.

## Tappa 00: Refactoring e Setup dell'ambiente

Il punto di partenza per lo sviluppo del simulatore è stato il codice sorgente fornito durante l'ultimo laboratorio del corso. L'obiettivo primario di questa fase iniziale è stato il refactoring del codice per transizionare a un ambiente sandbox minimale, base per le future implementazioni.
Le operazioni principali sono state:

- Semplificazione assoluta della scena, ora contiene solo un soggetto e un fondale.
- Rimozione arbitraria dei paradigmi di shading, mantenendo solo Phong
- Modifica dei path, cartelle e sistema di build.

![00](../00.png)

## Tappa 01: Nuovi controlli per il movimento della telecamera

In questa tappa ho cambiato il paradigma con cui si controlla la telecamera, dal movimento del mouse si e' passato ad un controllo a sei gradi di liberta' da tastiera:

- WASD per Pitch e Yaw (pan_tilt)
- EQ per il Roll
- LShift e LControl per avanzare o indietreggiare

I movimenti, che per ora si basano ancora sugli angoli di eulero, risultano indipendenti fra loro e dall'ambiente circostante.
E' facile notare alcune limitazioni di questa scelta, ad esempio il Gimbal Lock previene la possibilita' di eseguire una rotazione pitch di 360'.

## Tappa 02: Skybox, Luci e Texture base

In questo step fondamentale per lo sviluppo di un qualsiasi videogioco, viene definito un **ambiente**, con un fondale, `skybox` e un pavimento ricoperto da una sola texture spalmata. La prima ha richiesto la creazione di specifici vertex e fragment shader, mentre per il pavimento sono stati adattati quelli precedenti, differenziando materiale da texture. Infine l'unica luce non e' piu' vincolata al movimento della camera ma e' ancorata ad una posizione fissa del sole virtuale.

La `skybox` e' stata implementata utilizzando il campo nativo di OpenGL `GL_TEXTURE_CUBE_MAP`, iterando il caricamento su sei immagini `.png` scaricate dal web che costituiscono le sei facce del cubo. Per renderizzarla e' stato creato uno shader dedicato poiche' per lo sfondo non e' necessario avere un modello di illuminazione complesso (phong). Per forzare ciascuna faccia del cubo come sfondo infinitamente lontano, viene impostata la coordinata di profondita' z a 1.0 (valor massimo).
Infine il vertex shader della skybox calcola e comunica al fragment shader un vettore direzionale (X,Y,Z) necessario per campionare il colore di ciascun frammento del cubo.

Il pavimento ora e' ricoperto da una singola texture continua, per far cio' e' stato necessario modificare lo shader principale sostituendo il calcolo di un colore uniforme con quello proprio di ciascun pixel della texture, sfruttando la ripetizione automatica, `tiling` delle coordinate texture.

![02](../02.png)

## Tappa 03: Texture avanzate, decorazioni ambiente.
