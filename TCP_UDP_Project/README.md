# UDP & TCP Opdracht

In deze opdracht werken we met de **core transport protocols**: UDP en TCP.
We implementeren twee spelletjes:

*  **UDP** → "Wie is het dichtste?"
*  **TCP** → "Hoger of lager"

---

##  UDP – Wie is het dichtste?

📁 Te vinden in: `OEF1`

###  Beschrijving

In deze oefening maken we een UDP-server waar **2 clients** mee kunnen verbinden.
Zodra beide spelers verbonden zijn, start het spel automatisch.

###  Werking

* De server:

  * Genereert een **willekeurig getal**
  * Ontvangt gokken van beide clients (integers in ASCII)
  * Houdt per speler bij welk getal gestuurd werd
  * Bepaalt welk getal het **dichtst bij het juiste getal ligt**

* De clients:

  * Sturen hun gok naar de server
  * Kunnen meerdere pogingen doen binnen een bepaalde tijd

###  Extra logica

* De server werkt met **timeouts**
* Als een speler niet op tijd reageert, kan dit invloed hebben op het resultaat
* Omdat UDP **connectionless** is:

  * Berichten kunnen verloren gaan
  * Er is geen garantie op volgorde of aflevering

###  Resultaat

De speler met de **beste (dichtste) gok** wint het spel.

---

##  TCP – Hoger of Lager

📁 Te vinden in: `OEF2`

###  Beschrijving

In deze oefening maken we een TCP-server waar een client verbinding mee maakt om een klassiek **"hoger of lager"** spel te spelen.

###  Werking

* De server:

  * Genereert een **willekeurig getal**
  * Wacht op input van de client

* De client:

  * Stuurt een gok naar de server

* De server geeft feedback:

  *  "Lager" → als het juiste getal lager is dan de gok
  *  "Hoger" → als het juiste getal hoger is dan de gok
    
## Personen
  *  Arne Palmans

