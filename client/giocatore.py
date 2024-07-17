import socket
import sys

# MAIN

def lunghezza_stringa_con_terminatore(stringa):
    lunghezza = 0
    for carattere in stringa:
        if carattere == '\0':
            break
        lunghezza += 1
    return lunghezza

if __name__ == '__main__':
	string="referee"
	print("benvenuto giocatore, inserisci il tuo nome!")
	while string=="referee":
		string = input("---> ")
		if string == "referee":
			print("input non valido\n")
	print(f"hai inserito {string}")
	while True:
		conf=input("confermi il nome? [y/n]: ")
		if conf == "y":
			string+='\0'
			conn = ("127.0.0.1", 8080)
			try:
				s = socket.socket()
				s.connect(conn)
				s.send(string.encode())
				data=s.recv(1024)
				msg_intero = str(data, "latin-1")
				msg_size = lunghezza_stringa_con_terminatore(msg_intero)
				pid = msg_intero[:msg_size]
				if pid != "err":
					print(f"Sei entrato nella partita numero {pid}\n")
				else:
					print("coda piena, aspetta l'arrivo di un arbitro e riprova...")
			except socket.error as err:
				print(f"non ci sono partite disponibili: {err}, sto uscendo... \n")
			exit(0)
		else:
			print("input non valido\n")
		