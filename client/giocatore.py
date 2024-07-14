import socket
import sys

# MAIN

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
				pid=s.recv(1024)
				pidstr=pid.decode()
				print(f"Sei entrato nella partita numero {pidstr}\n")
			except socket.error as err:
				print(f"non ci sono partite disponibili: {err}, sto uscendo... \n")
			exit(0)
		else:
			print("input non valido\n")
		