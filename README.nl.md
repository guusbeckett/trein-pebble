# Trein

Een Pebble smartwatch app die live treinreissinformatie van Nederlandse Spoorwegen toont, inclusief een aftelklok tot je volgende trein vertrekt, vertrektijden en spoorinformatie.

## Functionaliteiten

- Realtime treinvertrektijden
- Aftelklok tot je volgende trein
- Spoorinformatie en vertragingen
- Automatische stationsdetectie op basis van je locatie
- Ondersteuning voor alle Pebble modellen (Aplite, Basalt, Chalk, Diorite, Emery, Flint)

## Vereisten

Om deze app te gebruiken heb je nodig:

1. Een Pebble smartwatch
2. De Pebble app geïnstalleerd op je smartphone
3. **Een NS API key** van het [NS API Portaal](https://apiportal.ns.nl/)

## Installatie

1. Installeer de app op je Pebble horloge via de Pebble app store of door sideloaden
2. **Configureer je NS API key** (vereist voor de werking van de app):
   - Open de Pebble app op je telefoon
   - Navigeer naar Settings → My Pebble Apps → Trein
   - Tik op "Settings"
   - Voer je NS API key in op de configuratiepagina
   - Sla de instellingen op

### Bemachtig een NS API Key

1. Ga naar het [NS API Portaal](https://apiportal.ns.nl/)
2. Maak een account aan of log in
3. Abonneer je op de benodigde APIs (NS App Stations API en Reisinformatie API)
4. Genereer een API key
5. Kopieer de key en plak deze in de app instellingen op je telefoon

## Gebruik

1. Open de Trein app op je Pebble horloge
2. De app detecteert automatisch de acht meest dichtstbijzijnde treinstations op basis van je locatie
3. Selecteer je vertrek- en bestemmingsstations
4. Bekijk aankomende treinen met vertrektijden, sporen en vertragingsinformatie
5. Gebruik de aftelklok om precies te zien hoeveel tijd je hebt tot je volgende trein, misschien kan je nog snel ff langs de Smullers

## Ontwikkeling

### Zelf de applicatie compileren

Dit is een Pebble SDK 3 project. Installeer eerst de [Pebble SDK](https://developer.repebble.com/sdk/) 
Na het installeren kan je de app complieren:

```bash
pebble build
```

### Mapstructuur

```
trein-pebble/
├── src/
│   ├── c/           # Native C code voor de app
│   └── pkjs/        # JavaScript code voor telefooncommunicatie
├── resources/       # App resources (iconen, etc.)
├── package.json     # Project configuratie
└── README.md
```

## Licentie

Dit programma is vrije software: je mag het herdistribueren en/of aanpassen onder de voorwaarden van de GNU General Public License zoals gepubliceerd door de Free Software Foundation, versie 3.

Zie de broncode headers voor volledige licentie-informatie.

## Auteur

Guus H. Beckett

## Bijdragen

Bijdragen zijn welkom! Voel je vrij om issues of pull requests in te dienen.

## Dankbetuigingen

- Eric Migicovsky voor het terugbrengen van Pebble
- Nederlandse Spoorwegen (NS) voor het aanbieden van de API
- De Pebble gemeenschap

## Ondersteuning

Als je problemen tegenkomt of vragen hebt:
- Controleer of je NS API key correct is geconfigureerd in de app instellingen
- Zorg ervoor dat je telefoon locatiediensten heeft ingeschakeld
- Verifieer dat je een actieve internetverbinding hebt

Voor bugs en feature requests, open een issue op GitHub.

---

[English version](README.md)
