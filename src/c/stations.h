/* 
 * This file is part of the Trein Pebble app distribution (https://github.com/guusbeckett/trein-pebble).
 * Copyright (c) 2025 Guus Beckett.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <pebble.h>

// Struct to hold station information
typedef struct {
  const char* code;
  const char* name;
} Station;

// Struct to map a letter to its list of stations
typedef struct {
    char letter;
    int start_index;
    int count;
} AlphabetIndex;

// The top 15 busiest stations in the Netherlands
static const Station top_stations[] = {
    {"UT", "Utrecht C"}, {"ASD", "Amsterdm C"}, {"RTD", "Rotterdm C"},
    {"GVC", "Den Haag C"}, {"SHL", "Schiphol"}, {"EHV", "Eindhovn C"},
    {"LEDN", "Leiden C"}, {"AH", "Arnhem C"}, {"HT", "Den Bosch"},
    {"AMF", "Amersfrt C"}, {"BD", "Breda"}, {"ZL", "Zwolle"},
    {"GN", "Groningen"}, {"NM", "Nijmegen"}, {"MT", "Maastricht"}
};
#define NUM_TOP_STATIONS (sizeof(top_stations) / sizeof(Station))

// --- Full list of stations, sorted alphabetically ---
static const Station all_stations[] = {
    {"ATN", "Aalten"},{"AC", "Abcoude"},{"AKM", "Akkrum"},{"RTA", "Alexander"},{"AMRN", "Alkmaar N"},{"AMR", "Alkmaar"},{"AML", "Almelo"},{"ALM", "Almere C"},{"APN", "Alphen"},{"AMF", "Amersfrt C"},{"ASA", "Amstel"},{"ASD", "Amsterdm C"},{"ASDZ", "Amsterdm Z"},{"ANA", "Anna Paulo"},{"APD", "Apeldoorn"},{"APG", "Appingedam"},{"AKL", "Arkel"},{"ARN", "Arnemuiden"},{"AH", "Arnhem C"},{"AHZ", "Arnhem Z"},{"ASN", "Assen"},{"SDTB", "Baanhoek"},{"BRN", "Baarn"},{"BF", "Baflo"},{"BRD", "Barendrcht"},{"BNC", "Barnevld C"},{"BNN", "Barnevld N"},{"BNZ", "Barnevld Z"},{"BDM", "Bedum"},{"BK", "Beek-E"},{"BSD", "Beesd"},{"BL", "Beilen"},{"BGN", "Bergen opZ"},{"BET", "Best"},{"BV", "Beverwijk"},{"ASB", "Bijlmer A"},{"BHV", "Bilthoven"},{"RTB", "Blaak"},{"HBZM", "Blauwe Zm"},{"BR", "Blerick"},{"BLL", "Bloemendl"},{"BDG", "Bodegraven"},{"BN", "Borne"},{"BSK", "Boskoop"},{"BHDV", "Boven-Har"},{"BKF", "Bovenk Flo"},{"BKG", "Bovenk-Gr"},{"BMR", "Boxmeer"},{"BTL", "Boxtel"},{"HMBV", "Brandevrt"},{"BD", "Breda"},{"BKL", "Breukelen"},{"HMBH", "Brouwhuis"},{"BMN", "Brummen"},{"ALMB", "Buiten"},{"BP", "Buitenpost"},{"BDE", "Bunde"},{"BNK", "Bunnik"},{"BSMZ", "Bussum Z"},{"LWC", "Camminghab"},{"HTNC", "Castellum"},{"CAS", "Castricum"},{"CVM", "Chevremont"},{"CO", "Coevorden"},{"DVC", "Colmschate"},{"CK", "Cuijk"},{"CL", "Culemborg"},{"DA", "Daarlervn"},{"DLN", "Dalen"},{"DL", "Dalfsen"},{"DTCH", "De Huet"},{"KLP", "De Klomp"},{"APDM", "De Maten"},{"AMRI", "de Riet"},{"DVNK", "De Vink"},{"DEI", "Deinum"},{"DDN", "Delden"},{"DTCP", "Delft Camp"},{"DT", "Delft"},{"DZW", "Delfzijl W"},{"DZ", "Delfzijl"},{"HT", "Den Bosch"},{"DLD", "Den Dolder"},{"GVC", "Den Haag C"},{"HDR", "Den Helder"},{"DN", "Deurne"},{"DV", "Deventer"},{"DID", "Didam"},{"DMNZ", "Diemen Z"},{"DMN", "Diemen"},{"DR", "Dieren"},{"HTO", "Dn Bosch O"},{"GV", "Dn Haag HS"},{"HDRZ", "Dn Heldr Z"},{"DTC", "Doetinchem"},{"DDZD", "Dordrcht Z"},{"DDR", "Dordrecht"},{"DB", "Driebergen"},{"DRH", "Driehuis"},{"DRP", "Dronryp"},{"DRON", "Dronten"},{"DVN", "Duiven"},{"DVD", "Duivendrt"},{"NMD", "Dukenburg"},{"EC", "Echt"},{"EDC", "Ede C"},{"ED", "Ede-Wag"},{"EEM", "Eemshaven"},{"EDN", "Eijsden"},{"EHV", "Eindhovn C"},{"EST", "Elst"},{"EMNZ", "Emmen Z"},{"EMN", "Emmen"},{"EKZ", "Enkhuizen"},{"ES", "Enschede"},{"EML", "Ermelo"},{"ESE", "Eschmarke"},{"ETN", "Etten-Leur"},{"GERP", "Europapark"},{"EGHM", "Eygelsh M"},{"EGH", "Eygelshov"},{"FWD", "Feanwâlden"},{"FN", "Franeker"},{"GDR", "Gaanderen"},{"GDM", "Geldermlsn"},{"GP", "Geldrop"},{"GLN", "Geleen O"},{"LUT", "Geleen-Lut"},{"HGLG", "Gezondhprk"},{"GZ", "Gilze-Rij"},{"GBR", "Glanerbrug"},{"GS", "Goes"},{"NMGO", "Goffert"},{"GO", "Goor"},{"GR", "Gorinchem"},{"GD", "Gouda"},{"GDG", "Goverwelle"},{"GBG", "Gramsbergn"},{"GK", "Grijpskerk"},{"GN", "Groningen"},{"GNN", "Groningn N"},{"GW", "Grou-Jirns"},{"HLM", "Haarlem"},{"HWZB", "Halfweg-Zw"},{"HDE", "'t Harde"},{"HDB", "Hardenberg"},{"HD", "Harderwijk"},{"GND", "Hardinxvld"},{"HRN", "Haren"},{"HLGH", "Harl Haven"},{"HLG", "Harlingen"},{"HK", "Heemskerk"},{"HAD", "Heemstede"},{"HR", "Heerenveen"},{"HWD", "Heerhugow"},{"HRLW", "Heerlen W"},{"HRL", "Heerlen"},{"HZE", "Heeze"},{"HLO", "Heiloo"},{"HNO", "Heino"},{"HM", "Helmond"},{"HMN", "Hemmen-D"},{"HGLO", "Hengelo O"},{"HGL", "Hengelo"},{"NMH", "Heyendaal"},{"HIL", "Hillegom"},{"HVS", "Hilversum"},{"HNP", "Hindeloopn"},{"HB", "Hoensbroek"},{"HVL", "Hoevelaken"},{"HOR", "Hol Rading"},{"ASHD", "Holendrcht"},{"HON", "Holten"},{"HFD", "Hoofddorp"},{"HGV", "Hoogeveen"},{"HGZ", "Hoogezand"},{"HKS", "Hoogkrspl"},{"HNK", "Hoorn Kers"},{"HN", "Hoorn"},{"HRT", "Horst-Sev"},{"HMH", "'t Hout"},{"HTN", "Houten"},{"SGL", "Houthem-St"},{"HDG", "Hurdegaryp"},{"IJT", "IJlst"},{"KPNZ", "Kampen Z"},{"KPN", "Kampen"},{"BZL", "Kapelle-Bi"},{"ESK", "Kennispark"},{"KRD", "Kerkrade C"},{"KTR", "Kesteren"},{"KBK", "Klarenbk"},{"KMR", "Klimmen-R"},{"ZDK", "Kogerveld"},{"KZ", "Koog Zaan"},{"KMW", "Koudum-M"},{"KBD", "Krabbendke"},{"KMA", "Krommenie"},{"KW", "Kropswolde"},{"KRG", "Kruiningen"},{"LAA", "Laan v NOI"},{"ZLW", "Lage Zwalu"},{"LG", "Landgraaf"},{"LLZM", "Lansingerl"},{"LDM", "Leerdam"},{"LW", "Leeuwarden"},{"LEDN", "Leiden C"},{"LDL", "Leiden Lam"},{"UTLR", "LeidscheRn"},{"ASDL", "Lelylaan"},{"LLS", "Lelystad C"},{"NML", "Lent"},{"LTV", "Lichtenv-G"},{"LC", "Lochem"},{"RLB", "Lombardije"},{"LP", "Loppersum"},{"UTLN", "Lunetten"},{"LTN", "Lunteren"},{"MZ", "Maarheeze"},{"MRN", "Maarn"},{"MAS", "Maarssen"},{"MTN", "Maastr. N"},{"MT", "Maastricht"},{"UTM", "Maliebaan"},{"MG", "Mantgum"},{"GVM", "Mariahoeve"},{"MRB", "Mariënberg"},{"MTH", "Martenshk"},{"HVSM", "Media Park"},{"MES", "Meerssen"},{"MP", "Meppel"},{"MDB", "Middelburg"},{"GVMW", "Moerwijk"},{"MMLH", "Mook-Molen"},{"ASDM", "Muiderprt"},{"ALMM", "Muziekwijk"},{"NDB", "Naarden-Bu"},{"NWK", "Nieuwerkrk"},{"NKK", "Nijkerk"},{"NM", "Nijmegen"},{"NVD", "Nijverdal"},{"NS", "Nunspeet"},{"NH", "Nuth"},{"NA", "Nw A'dam"},{"NVP", "Nw Vennep"},{"NSCH", "Nweschans"},{"OBD", "Obdam"},{"OT", "Oisterwijk"},{"ODZ", "Oldenzaal"},{"OST", "Olst"},{"OMN", "Ommen"},{"OTB", "Oosterbeek"},{"ALMO", "Oostvaard"},{"OP", "Opheusden"},{"OW", "Oss W"},{"O", "Oss"},{"APDO", "Osseveld"},{"ODB", "Oudenbosch"},{"UTO", "Overvecht"},{"OVN", "Overveen"},{"PMO", "Overwhere"},{"ALMP", "Parkwijk"},{"TPSW", "Passewaaij"},{"AMPO", "Poort"},{"AHPR", "Presikhaaf"},{"BDPB", "Prinsenbk"},{"PMR", "Purmerend"},{"PT", "Putten"},{"RAT", "Raalte"},{"RAI", "RAI"},{"MTR", "Randwyck"},{"RVS", "Ravenstein"},{"TBR", "Reeshof"},{"RV", "Reuver"},{"RH", "Rheden"},{"RHN", "Rhenen"},{"RSN", "Rijssen"},{"RSW", "Rijswijk"},{"RB", "Rilland-Ba"},{"RM", "Roermond"},{"RD", "Roodeschl"},{"RSD", "Roosendaal"},{"RS", "Rosmalen"},{"RTD", "Rotterdm C"},{"RTN", "Rotterdm N"},{"RTZ", "Rotterdm Z"},{"RL", "Ruurlo"},{"SPTN", "Santprt N"},{"SPTZ", "Santprt Z"},{"SSH", "Sassenheim"},{"SWD", "Sauwerd"},{"SGN", "Schagen"},{"SDA", "Scheemda"},{"SDM", "Schiedam C"},{"SOG", "Schin op G"},{"SN", "Schinnen"},{"SHL", "Schiphol"},{"CPS", "Schollevr"},{"AMFS", "Schothorst"},{"ASSP", "Scienceprk"},{"STD", "Sittard"},{"SDT", "Sliedrecht"},{"ASS", "Sloterdijk"},{"SKND", "Sneek N"},{"SK", "Sneek"},{"BSKS", "Snijdelwk"},{"STZ", "Soest Z"},{"ST", "Soest"},{"SD", "Soestdijk"},{"VSS", "Souburg"},{"HLMS", "Spaarnwde"},{"SBK", "Spaubeek"},{"HVSP", "Sportpark"},{"RTST", "Stadion"},{"ZLSH", "Stadshagen"},{"DDRS", "Stadspldrs"},{"STV", "Stavoren"},{"STM", "Stedum"},{"SWK", "Steenwijk"},{"EHS", "Strijp-S"},{"SRN", "Susteren"},{"SM", "Swalmen"},{"TG", "Tegelen"},{"TBG", "Terborg"},{"UTT", "Terwijde"},{"TL", "Tiel"},{"TBU", "Tilburg Un"},{"TB", "Tilburg"},{"WADT", "Triangel"},{"TWL", "Twello"},{"UTG", "Uitgeest"},{"UHZ", "Uithuizen"},{"UHM", "Uithuizerm"},{"UST", "Usquert"},{"UT", "Utrecht C"},{"UTVR", "VaartscheR"},{"VK", "Valkenburg"},{"VSV", "Varsseveld"},{"AVAT", "Vathorst"},{"VDM", "Veendam"},{"VNDC", "Veenendl C"},{"VNDW", "Veenendl W"},{"VP", "Velp"},{"AHP", "Velperprt"},{"VL", "Venlo"},{"VRY", "Venray"},{"VLB", "Vierlingsb"},{"VTN", "Vleuten"},{"VS", "Vlissingen"},{"VDL", "Voerendaal"},{"VB", "Voorburg"},{"VH", "Voorhout"},{"VST", "Voorschtn"},{"VEM", "Voorst-E"},{"VD", "Vorden"},{"VZ", "Vriezenvn"},{"VHP", "Vroomshoop"},{"VG", "Vught"},{"WADN", "Waddinxv N"},{"WAD", "Waddinxvn"},{"WFM", "Warffum"},{"WT", "Weert"},{"WP", "Weesp"},{"WL", "Wehl"},{"PMW", "Weidevenne"},{"DWE", "Westereen"},{"WTV", "Westervrt"},{"WZ", "Wezep"},{"WDN", "Wierden"},{"WC", "Wijchen"},{"WH", "Wijhe"},{"WS", "Winschoten"},{"WSM", "Winsum"},{"WWW", "Wintersw W"},{"WW", "Winterswk"},{"WD", "Woerden"},{"WF", "Wolfheze"},{"WV", "Wolvega"},{"WK", "Workum"},{"WM", "Wormerveer"},{"YPB", "Ypenburg"},{"ZD", "Zaandam"},{"ZZS", "Zaanse S."},{"ZBM", "Zaltbommel"},{"ZVT", "Zandvoort"},{"ZA", "Zetten-And"},{"ZV", "Zevenaar"},{"ZVB", "Zevenbergn"},{"ZTM", "Zoetermeer"},{"ZTMO", "Zoetermr O"},{"ZB", "Zuidbroek"},{"ZH", "Zuidhorn"},{"UTZL", "Zuilen"},{"ZP", "Zutphen"},{"ZWD", "Zwijndrcht"},{"ZL", "Zwolle"}
};
#define NUM_STATIONS (sizeof(all_stations) / sizeof(Station))

// --- Alphabetical Index ---
// This tells the app where each letter's stations start in the big list and how many there are.
static const AlphabetIndex alphabet_index[] = {
    {'A', 0, 21}, {'B', 21, 38}, {'C', 59, 8}, {'D', 67, 37},
    {'E', 104, 17}, {'F', 121, 2}, {'G', 123, 19}, {'H', 142, 42},
    {'I', 184, 1}, {'K', 185, 15}, {'L', 200, 18}, {'M', 218, 18},
    {'N', 236, 10}, {'O', 246, 15}, {'P', 261, 7}, {'R', 268, 19},
    {'S', 287, 35}, {'T', 322, 8}, {'U', 330, 5}, {'V', 335, 23},
    {'W', 358, 21}, {'Y', 379, 1}, {'Z', 380, 15}
};
#define ALPHABET_INDEX_COUNT (sizeof(alphabet_index) / sizeof(AlphabetIndex))