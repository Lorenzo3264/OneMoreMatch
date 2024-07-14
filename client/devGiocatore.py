import socket
import sys

# MAIN

giocatori = {
    '0':"Unidraulico",
    '1':"Duein",
    '2':"Trevor",
    '3':"Quasimodo",
    '4':"Cinzio",
    '5':"Seimone",
    '6':"Seth",
    '7':"Otto",
    '8':"Novembro",
    '9':"Diego",
}

if __name__ == '__main__':
	i=0
	while i<8:
		conn = ("127.0.0.1", 8080)
		try:
			s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			s.connect(conn)
			string = giocatori[str(i)]
			s.send(string.encode())
			i += 1
			pid=s.recv(10)
			pidstr=pid.decode()
			print(f"Sei entrato nella partita numero {pidstr}\n")
			s.close()
		except socket.error as err:
			print(f"non ci sono partite disponibili: {err}, sto uscendo... \n")
			exit(0)
		