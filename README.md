# OfflineMessengerRC
Sa se dezvolte o aplicatie client/server care sa permita schimbul de mesaje intre utilizatori care sunt conectati si sa ofere functionalitatea trimiterii mesajelor si catre utilizatorii offline, acestora din urma aparandu-le mesajele atunci cand se vor conecta la server. De asemenea, utilizatorii vor avea posibilitatea de a trimite un raspuns (reply) in mod specific la anumite mesaje primite. Aplicatia va oferi si istoricul conversatiilor pentru si cu fiecare utilizator in parte.
# How to use this app
in client: un mesaj trimis cu prefixul '/' este considerat o comanda catre server, de acolo in server se trateaza comanda daca este definita.
Comenzile acceptate: 
- "/register" creaza un utilizator nou.
- "/login <username>" logheaza utilizatorul cu numele <username>.
- "/send <destinatie> <mesaj>" daca esti logat poti trimite un mesaj catre alt username definit ca si destinatie, utilizatorul <destinatie> daca este logat primeste mesajul imediat sau daca nu este logat pe server va primi mesajul imediat ce se logheaza.
-"/history <username>" este comanda care afiseaza istoricul conversatiei cu utilizatorul <username>, trebuie desigur sa fii logat pentru a folosi aceasta comanda
- "/reply <username> <idMesaj> <mesajulRaspuns>" aceasta comanda va trimise un mesaj catre <username> in formatul "<usernameCurent> Replied to <username>:<mesajul de la idMesaj>:<mesajulRaspuns>", se foloseste de functionabilitatea de la comanda "/send..." asa ca mesajul este trimis si utilizatorilor offline.
