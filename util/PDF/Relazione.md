# Relazione Progetto - Baldini Filippo S6393212

Relazione del progetto di Fondamnti di Computer Grafica. \
Versionamento a tappe dello sviluppo di un semplice simulatore di volo.

## Tappa 00: Refactoring e Setup dell'ambiente

Il punto di partenza per lo sviluppo del simulatore è stato il codice sorgente fornito durante l'ultimo laboratorio del corso. L'obiettivo primario di questa fase iniziale è stato il refactoring del codice per transizionare a un ambiente sandbox minimale, base per le future implementazioni.
Le operazioni principali sono state:

- Semplificazione assoluta della scena, ora contiene solo un soggetto e un pavimento.
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

Per finalizzare l'ambiente di gioco, in questa tappa sono state aggiunte le seguenti modifiche:

- Texture del suolo sostituita tramite una blend map da quattro texture distinte (Contesto urbano, residenziale, asfalto e acqua).
- Inserimento di un numero arbitrario di rettangoli texturizzati, per rappresentare dei palazzi.
- Un'ulteriore modifica alla luce, ora ha un tono piu' caldo e rispecchia forse meglio quella solare.

Per decorare maggiormente il suolo ho scelto di utilizzare una blend map, ossia un'immagine 2D descritta da soli colori RGB puri (e il nero) grazie alla quale il fragment shader e' in grado di posizionare tramite la funzione `mix(A, B, C.ch)`, le texture fornitegli in input.
Ho scelto questa strada principalmente perche' mi affascinava come concetto, l'alternativa che avevo valutato era quella di considerare il suolo come un enorme griglia e posizionare in ogni cella una specifica texture. Quindi `main_shader.frag` ora e' in grado di applicare ripetutamente una certa texture in base al colore della blend map ottenuta da `GIMP`, cosi' creando un piacevole effetto di sovrapposizione seamless delle texture.

La classe che ha subito la maggior parte delle modifiche e' stata `Scene`, infatti sono stati definiti dei nuovi campi per rappresentare la blend map e un vettore per le diverse texture poi posizionate sul suolo. Nel nuovo metodo `init_ground` non faccio altro che fornire le texture, mentre nella `draw_floor`, ora modificata, eseguo la Bind in GPU. Come nella tappa precedente le texture sono considerate come oggetti della classe `Texture2D`.

In secondo luogo ho deciso di aggiungere all'ambiente ora texturizzato dei semplici elementi, cosi' da poter dare un'idea di scala al mio mondo virtuale.
Similmente al caso precedente, definisco i campi per lo shader, in questo caso creo due vettori, uno per degli oggetti di tipo `building`, associazione fra modello e texture e un secondo per le texture dei palazzi. In questo momento ho valutato due scenari possibili, un posizionamento manuale dei palazzi, oppure sfruttare ulteriormente la blend map per far si che se il suolo e' di un certo tipo allora aggiungo un certo palazzo piuttosto che un altro. La mia scelta e' ricaduta sulla prima possibilita' in quanto era quella che si allineava maggiormente alla mia idea, e risultava anche meno complessa da programmare, seppur l'analisi della blend map sarebbe potuta tornar utile anche in futuro, ad esempio per comportamenti specifici per zona (ad esempio rumori del mare, urbani, etc.).

Quindi ho inserito un totale di 23 palazzi, con posizione e dimensioni definite nel metodo prolisso `init_buildings`. Con una serie di tentativi, valutando arbitrariamente la scena, ottengo cosi' un risultato apprezzabile. Inizialmente avevo deciso di mantenere temporaneamente lo stesso shader condiviso per il suolo e i palazzi; questo e' ottenuto con un valore booleano inviato allo shader che valuta cosa sta manipolando. Non era una soluzione ottimale, infatti nel metodo `draw` avevo dovuto inserire

```cpp
if (blendmap != nullptr) blendmap->Bind(0);
for (int j = 0; j < ground.size(); ++j) {
    ground[j]->Bind(j + 1);
}
```

questo perche' per come ho impostato le texture:

    0 -> skybox (cubemap), blendmap (sampler2D)
    1 -> ground1
    2 -> ground2
    [...]
    4 -> ground4
    5 -> building1
    [...]

succedeva che chiamando prima il metodo `draw_buildings` eseguivo correttamente la bind delle texture dall'indice successivo a ground.size(). Ma ottenevo il warning:
`UNSUPPORTED (log once): POSSIBLE ISSUE: unit 2 GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero texture because texture unloadable` indica che lo shader si aspetta gia' le texture associate al suolo (fornitegli con la draw_floor successiva) e per questo eseguivo una bind preventiva.

Non soddisfatto dalla soluzione trovata ho separato lo shader in due, uno sempre con paradigma phong per il suolo, e uno flat per i palazzi (che alla fine non sono altro che parallelepipedi). La soluzione e' stata trovata eseguendo un refactoring del codice, in particolare ora le classi `Camera` e `Lights` sono separate dallo shader, cosi' facendo posso indipendentemente chiamarle sul suolo e sui palazzi. `draw_cube` ha ricevuto anch'esso una semplice modifica, ora calcola lui le location in GPU delle matrici.

Infine ho modificato, in una serie di tentativi, i valori della luce fino a che non ho trovato una soluzione di mio garbo.

![03](../03.png)

## Tappa 04: Fisica di base

In questa tappa l'obbiettivo era quello di aggiungere una serie di comportamenti e feedback per simulare il volo di un aereo e non di una astronave nel vuoto.

Innanzitutto ho dovuto scegliere quale livello di simulazione implementare. Inizialmente mi ero affidato ad un tutorial online che, tramite un modello dinamico (basato su forze, superfici e dati realistici), andava a simulare in modo piuttosto fedele il comportamento di un aereo nella nostra atmosfera. Tuttavia questo approccio risultava eccessivamente complesso e fuori dalla mia portata, specialmente in quanto dati e metodi implementati facevano affidamento su un ambiente di gioco molto piu' sviluppato. In futuro si potrebbe pensare di rivalutarlo.

Dunque ho optato per un approccio piu' arcade-like, nel quale viene implementata una manetta virtuale a cinque stadi, tramite la quale l'aereo accelera o decelera. Questo effetto si ottiene in `physics.hh` grazie alla variabile `speed-difference` che va a calcolare un valore positivo (o negativo) che poi si applica tramite l'equazione cinematica del moto rettilineo per calcolare la velocita' del velivolo nello spazio.

Il modello da me implementato simula la gravita'tramite un sistema basato sulla somma di tre vettori associati all'aereo:

- Vettore direzione, dato dall'orientamento spaziale della telecamera.
- Vettore della forza normale, ortogonale direzionato verso l'alto rispetto all'aereo, associato ad un valore di lift ossia, quanto l'aereo e' in grado di combattere la foza di gravita' in base alla sua velocita' corrente.
- Vettore della forza peso, in questo caso corrispondente solo alla forza di gravita' in quanto il peso e' unitario, direzionato verso il suolo.

da notare come, mentre il vettore normale ruota con l'aereo, il vettore della forza peso rimane fisso, correttamente, a puntare verso il suolo.

Questo permete una simulazione abbastanza banale di un corpo nello spazio che si muove ed e' soggetto ad una forza costante che lo attrae al suolo ed una resistenza imposta da lui che varia dinamicamente man mano che acquisisce velocita'.

Inoltre e' stato definito un metodo `attach_to` all'interno della classe `Camera` che permette l'associazione fra camera (angoli di eulero) e target (quaternione).
Infine ho definito un metodo `check_collision()` che restituisce `true` se si superano dei limiti (suolo, cielo), per ora si possono attraversare i palazzi.

[Link: Modello piu' complesso.](https://www.jakobmaier.at/posts/flight-simulation/)

## Tappa 05: Controlli avanzati

Rispetto alla versione precedente del gioco e' immediato notare alcuni cambiamenti nella sensibilita' e nell'effetto dei controlli del velivolo.

Sono stati definiti nuovi comportamenti per rendere piu' uniformi i movimenti controllati da tastiera, con un ragionamento simile a quello applicato per l'accelerazione. Ora i controlli hanno un target desiderato e un valore che con lo scorrere del tempo lo raggiunge. Oltre a rendere questi movimenti piu' fluidi, ho aggiunto anche un fattore di allineamento delle rotazioni. Per `pitch` e `roll` questo si ottiene calcolando la differenza fra la rotazione corrente e un valore calcolato dal prodotto fra una costante e il valore y del vettore associato al rilascio del pulsante. Cosi' facendo il valore della rotazione del velivolo andra' man mano a diminuire e nel mentre subira' anche un movimento opposto che dara' l'effetto di auto-align. Ricordando alcuni simulatori che ho provato in passato, il comando di `yaw`, ottenuto premendo i pedali del velivolo, ha un rientro molto piu' brusco, talvolta oscillante. La mia implementazione cerca di avvicinarsi a tale comportamento, facendo si che piu' a lungo il comando viene tenuto premuto meno forte sara' la spinta ritornata da una sorta di molla virtuale che si scarica quando il comando viene lasciato.

In questa tappa ho anche completato la funzione `check_collision()`, che adesso calcola per ogni palazzo la dimensione della base e l'altezza, e le confronta con la posizione dell'aereo. Questo controllo viene effettuato ad ogni frame per ogni palazzo. Risulterebbe molto poco efficiente se,

- I palazzi diventassero centinaia,
- I palazzi fossero molto distanti fra loro.

probabilmente esistono metodi piu' intelligenti per farlo, ad esempio controllare i palazzi a settori, solo se l'aereo e' sotto una certa altitudine, ma come soluzione penso sia attualmente adatta al mio progetto.

![05 GIF](../05.gif)

## Tappa 06: HUD

Concluso lo sviluppo della fisica di gioco, a questo punto mi sono dedicato alla creazione di un HUD.
Inizialmente l'idea era quella di proiettare sulla telecamera delle immagini bidimensionali, quindi sfruttando `SFML` avevo creato una classe che aveva il semplice compito di mostrare a schermo la velocita' del velivolo:

```cpp
class Hud {
    private:
        sf::Font font;
        sf::Text hud_text;
    public:
        Hud (const std::string& path) : hud_text(font) {
            if (!font.openFromFile(path)) {
                std::cerr << "Failure: error during SFML Font Loading." << std::endl;
            }
            // text properties
            hud_text.setFont(font);
            hud_text.setCharacterSize(24);
            hud_text.setFillColor(sf::Color::White);
            // hud positioning
            sf::Vector2f t_pos = sf::Vector2f(0.0f, 0.0f);
            hud_text.setPosition(t_pos);
        }
        void update (fcg::Airplane airplane) {
            float speed = static_cast<int>(airplane.get_speed()); // to int to avoid decimal
            hud_text.setString(std::to_string(speed) + "km/h\n");
        }
        void draw(sf::RenderWindow& window) {
            window.resetGLStates(); // pauses and resumes OpenGL rendering
            window.draw(hud_text);
        }
};
```

Tuttavia, prima cercando su forum online, poi chiedendo direttamente ad un LLM (Gemini, Claude), questo approccio non e' implementabile. Dalla mia comprensione, sembra che la versione di `OpenGL` supportata da `MacOS` risulta incompatibile con alcune funzioni di libreria `SFML` e dunque far collaborare le due per un rendering misto 2D/3D risulta impossibile.

A questo punto ho avuto l'idea di usare un approccio diverso, definire direttamente in `OpenGL` dei sottilissimi quadrati, texturizzati in modo opportuno, a distanza minima dalla camera, cosi' da ottenere una sorta di HUD.

Partendo da questa idea, e costruendo una serie di strumenti, adotto lo stesso approccio dei `building`:

```cpp
// hud elements
struct hud_element {
    glm::vec3 size;
    glm::vec3 pos;
    Texture2D* texture;
};
std::vector<hud_element> hud;
std::vector<Texture2D*> hud_t;
```

Definendo poi un semplicissimo shader di supporto, nel metodo `init_hud`, inizializzo le posizioni e le texture dei singoli strumenti.

Quindi il mio HUD risulta essere una serie di `cube.off` scalati, ruotati, traslati, impilati l'uno sopra l'altro.

Per ottenere queste immagini, sono partito dall'HUD del gioco di riferimento, `Microsoft Flight Simulator 2000`, e sempre con `GIMP` ho isolato e adattato alcuni strumenti da animare. In totale sono quattro:

- Indicatore dello `yaw`
- Orizzonte Virtuale
- Indicatore della `speed`
- Cloche

Ognuno di questi ha i relativi comportamenti descritti nel metodo `draw_hud ()`. Il piu' difficile da sviluppare e' stato l'orizzonte virtuale. Realisticamente sarebbe dovuta essere una sfera texturizzata, in grado di indicare la posizione assoluta del suolo rispetto al velivolo, mentre nel mio caso e' solo una `.png` che ruota in base all'asse di `roll`. Quest'ultimo e' ottenuto con un ragionamento simile a quello del metodo `attach_to` della camera, infatti preleva dal quaternione `orientation` i dati necessari per calcolare di quanto l'aereo risulta inclinato.

NB, la funzione di libreria `glm::roll(quat q)` non riusciva a interpretare correttamente situazioni di `Gimbal_lock`.

Mi rendo conto che questa soluzione non e' ottimale, disegnare l'hud nello stesso contesto tridimensionale del paesaggio non penso sia una scelta architetturalmente corretta. Infatti in alcune situazioni, ad esempio quando il velivolo si avvicina pericolosamente ad un palazzo, quest'ultimo risultera sovrapposto all'HUD generando conflitti visivi. Tuttavia nella maggior parte dei casi, l'HUD e' stabile.

![06 GIF](../06.gif)

## Tappa 07: Suoni

Utilizzando la componente `sf::Sound` di `SFML`, procedo ad implementare diversi rumori e feedback sonori nel mio gioco. Seguendo la wiki ufficiale della libreria, creo una classe `Audio` e la popolo con costruttore, metodi pubblici e diversi `sf::SoundBuffer`, uno per ogni suono. Probabilmente avrei potuto risparmiarne uno, sostituendo un certo suono con quello dello schianto.
A termine dello sviluppo avro':

- Rumore dinamico del motore
- Rumore di click della manetta
- Rumore di schianto
- Rumore Ambientale

Dunque, mentre i primi tre sono semplici `.wav` caricati in RAM, l'ultimo e' una traccia che, tramite `sf::Music`, viene letta e mandata in loop direttamente dal file audio.

Come per accelerazione e rotazioni varie, gestisco l'aumento e diminuzione del pitch del suono con una certa crescita legata ad un `float dt`, in questo caso preso dal clock globale del programma.

L'aumento di velocita' del velivolo e' simulato grazie ad un semplice aumento del pitch, che fa si che quando aumenta la velocita' il suono sia piu' acuto, viceversa se diminuisce, sara' piu' grave. Ho modificato le `handle` della manetta, ora riproducono un suono per ciascuno step, ed una per indicare il raggiungimento del livello massimo (minimo).

Forse la parte piu' complessa e' stata legare la variazione del pitch, oltre che alla manipolazione della manetta, anche all'inclinazione dell'asse di pitch dell'aereo. Avrei potuto sviluppare questo comportamento legandolo unicamente all'audio, pero' ho pensato fosse piu' corretto andare direttamente ad aggiungere prima questo in `physics_adv.hh` cosi' ottenendo una soluzione piu' pulita.

```cpp
    glm::vec3 fw_dir = orientation * glm::vec3(0.0f, 0.0f, -1.0f);

    float engine_power = (throttle_level / WEIGHT_FEEL) * MAX_SPEED; // old target velocity
    float inc_effect = -fw_dir.y * 10.0f; //.y [-1. 1], 1 = straight up, -1 = straight down

    float target_velocity = engine_power + inc_effect;
    target_velocity = glm::max(0.0f, target_velocity);
```

Come per l'orizzonte virtuale, calcolo il valore di inclinazione rispetto al terreno grazie al valore y del vettore frontale (direzione dell'aereo) e lo utilizzo per creare uno scalare da aggiungere o sottrarre alla velocita' obbiettivo. Il resto della fisica rimane invariata.

Infine aggiungo nel ciclo principale un controllo migliorato per `check_collision()` che ora riprodurra' un suono e visualizzera' lo schermo nero. `sf::sleep(sf::seconds(float s))` e' necessario per dare il tempo al programma di riprodurre il suono.
