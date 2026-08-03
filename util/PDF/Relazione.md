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
