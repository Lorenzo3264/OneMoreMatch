# OneMoreMatch
 Progetto di Laboratorio di Sistemi Operativi, traccia "La Partita"

## Istruzioni per lanciare l'applicazione:
- Prerequisiti:
	- Installare l'ultima versione di python.
	- Installare pillow eseguendo `pip install pillow` da terminale.
	- Installare Docker con estensione compose.
- Istruzioni di lancio:
	- Avviare il server lanciando `docker compose up -d` da terminale nella cartella del repository,
	- Lanciare i client giocatori dalla sua cartella (quindi `cd client` dal repository) attraverso l'uso di `python giocatore.py`
	- (OPZIONALE) puoi lanciarne otto con un solo script `python devGiocatore.py`
	- Avviare successivamente il client arbitro dalla sua cartella (quindi `cd client` dal repository) eseguendo `python client.py`

 Se dovessero verificarsi errori durante il lancio seguire le istruzioni a schermo.
