Nume: Ionescu Elena
Grupa: 336CA

Tema 3 - Loader de Executabile

Organizare

  In cadrul acestei teme, am completat functiile so_execute si so_init_loader
din fisierul loader.c. In so_execute, pentru fiecare segment, am alocat memorie pentru
campul data si i-am facut cast la o structura creata de mine, in care voi retine
un buffer cu numarul paginilor si o variabila ce retine cate pagini sunt in acel buffer.
Am facut acest lucru pentru a tine evidenta paginilor care au fost mapate deja in memorie.
In continuare, initializez cele doua campuri din structura.
  In functia so_init_loader, creez un hadler care va fi rulat daca primesc un semnal
de SIGSEGV si ii asociez o functie de handler care va prelucra semnalul primit.
In cadrul acelei functii, daca primesc un semnal de SIGSEGV, verific din ce segment
face parte eroarea respectiva, apoi verific daca pagina respectiva a mai fost sau nu 
mapata in memorie. In cazul in care pagina a mai fost mapata in memorie, rulez 
hadler-ul default, iar in caz contrar, o mapez in memorie si copiez datele din 
fisier in pagina. Daca primesc orice alt semnal in afara de SIGSEGV, rulez handler-ul
default.
  Consider ca tema a fost utila, deoarece m-a ajutat sa inteleg cum se pun datele
in memorie si cum lucrez cu paginile virtuale. Are sens acum faptul ca aceasta memorie
virtuala ofera procesului iluzia ca dispune de intreg spatiul de memorie si ca se 
comporta ca si cand ar rula singur pe calculator.

Implementare

  In cadrul acestei teme, am implementat intreaga cerinta.

Dificultati de implementare
	
  Am avut dificultati cand am copiat datele din fisier in paginile virtuale.
Primeam segmentation fault la rulare, deoarece nu mi se copiau datele bine, pentru ca
nu pusesem bine conditia de verificare daca mai am date de copiat din fisier in memoria virtuala.

Lucruri interesante descoperite pe parcurs
	Mi se pare interesant cum putem copia date in memorie, folosind pagini virtuale.

Tema se compileaza folosind comanda make din cadrul Makefile-ului din skeletul temei.
In urma compilarii, se genereaza o biblioteca dinamica libso_loader.so.

Bibliografie

https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-06
https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-04
https://www.mkssoftware.com/docs/man5/siginfo_t.5.asp
