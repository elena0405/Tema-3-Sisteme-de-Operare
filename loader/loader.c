/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "exec_parser.h"

static so_exec_t *exec;

// Aici retin handler-ul default.
static struct sigaction handler_default;
static int file_descriptor;

// Voi folosi aceasta structura pentru a retine datele unui segment.
struct segment {
	// In acest camp retin numarul de pagini.
	int numar_pagini;
	// In acest camp retin un vector de pagini, de dimensiune numar_pagini.
	int *buffer_pagini;
};

static void handler_function(int signum, siginfo_t *info, void *context) {
	void *adresa_seg_fault;
	void *capat_stanga, *capat_dreapta;
	void *adresa_de_mapare;
	so_seg_t segment;
	struct segment *pagini_segment;
	int index_segment, index_pagini, nr_pagina;
	int pagina_urmatoare, dim_ramasa;
	int nr_segmente = exec->segments_no;
	int nr_pagini, rc;
	int dim_pagina = getpagesize();
	void *pagina;

	switch(signum) {
		case SIGSEGV:
			// Daca semnalul curent este SIGSEGV, caut adresa care a determinat acest semnal.
			// Iterez mai intai prin fiecare segment.
			index_segment = 0;
			// Retin adresa care a provocat semnalul.
			adresa_seg_fault = info->si_addr;
			while (index_segment < nr_segmente) {
				// Retin segmentul curent intr-o variabila.
				segment = exec->segments[index_segment];
				// Retin capetele in care fac cautarea pentru segmentul curent.
				capat_stanga = (void*)segment.vaddr;
				capat_dreapta = (void*)segment.vaddr + segment.mem_size;

				// Verific daca adresa se afla in intervalul selectat.
				if (adresa_seg_fault >= capat_stanga && adresa_seg_fault <= capat_dreapta) {
					// Daca da, retin structura de pagini aferenta segmentului.
					pagini_segment = (struct segment*)segment.data;
					break;
				}

				index_segment++;
			}
			
			// Daca indexarea prin segmente nu s-a terminat, inseamna ca a intrat in if-ul
			// din while, deci am gasit segmentul cu pricina.
			if (index_segment < nr_segmente) {
				// Retin numarul paginii.
				nr_pagina = ((uintptr_t) adresa_seg_fault - segment.vaddr) / dim_pagina;
				nr_pagini = pagini_segment->numar_pagini;

				// Verific daca a fost alocat spatiu de memorie pentru vectorul de pagini.
				if (pagini_segment->buffer_pagini == NULL) {
					// Daca nu, atunci aloc spatiu.
					pagini_segment->buffer_pagini = calloc(segment.mem_size / dim_pagina + 1, sizeof(int));
					if (pagini_segment->buffer_pagini == NULL) {
						printf("Eroare la alocarea vectorului pagini_segment!\n");
						return;
					}

					// Mapez o noua pagina in memorie.
					adresa_de_mapare = capat_stanga + nr_pagina * dim_pagina;
					pagina = mmap(adresa_de_mapare, dim_pagina, PROT_WRITE | PROT_READ, 
									MAP_ANONYMOUS | MAP_SHARED | MAP_FIXED, file_descriptor, 0);

					// Daca maparea nu s-a facut cu succes, trimit un mesaj de eroare.
					if (pagina == MAP_FAILED) {
						printf("Eroare la maparea unei paginii %d!\n", nr_pagina);
						return;
					}

					pagini_segment->buffer_pagini[0] = nr_pagina;
					pagini_segment->numar_pagini++;

					// Verific daca mai am date in fisier ce trebuie adaugate.
					if (segment.file_size / dim_pagina >= nr_pagina) {
						// Daca da, ma pozitionez in fisier la offset-ul.
						rc = lseek(file_descriptor, segment.offset + nr_pagina * dim_pagina, SEEK_SET);
						if (rc == -1) {
							printf("Eroare la pozitionarea in fisier!\n");
							return;
						}

						// Verific daca datele din fisier depasesc lungimea paginii curente.
						pagina_urmatoare = nr_pagina + 1;
						if (segment.file_size / dim_pagina >= pagina_urmatoare) {
							// Daca da, umplu pagina curenta cu date.
							rc = read(file_descriptor, pagina, dim_pagina);
							if (rc == -1) {
								printf("Eroare la citirea din fisier!\n");
								return;
							}
						} else {
							// Daca datele ramase in fisier incap in pagina, copiez restul datelor
							// in pagina.
							dim_ramasa = segment.file_size - nr_pagina * dim_pagina;
							rc = read(file_descriptor, pagina, dim_ramasa);
							if (rc == -1) {
								printf("Eroare la citirea din fisier!\n");
								return;
							}
						}
					}

					// Schimb permisiunile paginii la cele dorite.
					rc = mprotect(pagina, dim_pagina, segment.perm);
					if (rc == -1) {
						printf("Eroare la schimbarea permisiunilor unei pagini!\n");
						return;
					}
				} else {
					// Daca vectorul de pagini nu este NULL, atunci verific daca pagina 
					// a mai fost deja mapata.
					index_pagini = 0;

					while (index_pagini < nr_pagini) {
						if (nr_pagina == pagini_segment->buffer_pagini[index_pagini]) {
							break;
						}
						
						index_pagini++;
					}
					
					// Daca pagina nu a mai fost deja mapata, o mapez in memorie.
					if (index_pagini == nr_pagini) {
						// Retin adresa la care trebuie sa o mapez.
						adresa_de_mapare = capat_stanga + nr_pagina * dim_pagina;
						pagina = mmap(adresa_de_mapare, dim_pagina, PROT_WRITE | PROT_READ, 
										MAP_ANONYMOUS | MAP_SHARED | MAP_FIXED, file_descriptor, 0);
						// Verific daca maparea a avut loc cu succes.
						if (pagina == MAP_FAILED) {
							printf("Eroare la maparea unei paginii %d!\n", nr_pagina);
							return;
						}
						
						// Retin numarul paginii in vectorul de pagini aferent segmentului curent.
						pagini_segment->buffer_pagini[pagini_segment->numar_pagini] = nr_pagina;
						pagini_segment->numar_pagini++;

						// Verific daca mai am date in fisier ce trebuie adaugate.
						if (segment.file_size / dim_pagina >= nr_pagina) {
							// Daca da, ma pozitionez in fisier la offset-ul.
							rc = lseek(file_descriptor, segment.offset + nr_pagina * dim_pagina, SEEK_SET);
							if (rc == -1) {
								printf("Eroare la pozitionarea in fisier!\n");
								return;
							}

							// Verific daca datele din fisier depasesc lungimea paginii curente.
							pagina_urmatoare = nr_pagina + 1;
							if (segment.file_size / dim_pagina >= pagina_urmatoare) {
								// Daca da, umplu pagina curenta cu date.
								rc = read(file_descriptor, pagina, dim_pagina);
								if (rc == -1) {
									printf("Eroare la citirea din fisier!\n");
									return;
								}
							} else {
								// Daca datele ramase in fisier incap in pagina, copiez restul datelor
								// in pagina.
								dim_ramasa = segment.file_size - nr_pagina * dim_pagina;
								rc = read(file_descriptor, pagina, dim_ramasa);
								if (rc == -1) {
									printf("Eroare la citirea din fisier!\n");
									return;
								}
							}
						}

						// Schimb permisiunile paginii la cele dorite.
						rc = mprotect(pagina, dim_pagina, segment.perm);
						if (rc == -1) {
							printf("Eroare la schimbarea permisiunilor unei pagini!\n");
							return;
						}
					} else {
						// Daca pagina a mai fost deja mapata, rulez handler-ul default.
						handler_default.sa_sigaction(signum, info, context);
					}
				}
			} else {
				// In caz contrar, se ruleaza handler-ul default.
				handler_default.sa_sigaction(signum, info, context);	
			}

			break;
		default:
			// Daca am orice alt semnal in afara de SIGSEGV, rulez handler-ul default.
			handler_default.sa_sigaction(signum, info, context);
	}
}

int so_init_loader(void)
{
	/* TODO: initialize on-demand loader */
	// In aceasta variabila retin valoarea intreaga intoarsa de functiile pe 
	// care le apelez, pentru a testa aparitia erorilor.
	int rc;
	// Aici retin handler-ul pentru semnalul de segmentation fault.
	struct sigaction segv_handler;

	// Setez valoarea flagului la SA_SIGINFO pentru a folosi handler-ul
	// pe care il voi specifica in campul sa_sigaction al variabilei segv_handler.
	segv_handler.sa_flags = SA_SIGINFO;
	// Asociez handler-ului segv_handler functia handler_function.
	segv_handler.sa_sigaction = handler_function;

	// Initializez masca de biti pentru semnale cu 0, pentru handler_default.
	rc = sigfillset(&handler_default.sa_mask);
	// Daca apar probleme, afisez un mesaj de eroare si ies din functie.
	if (rc == -1) {
		printf("Eroare la initializarea mastii de biti pentru handler_default!\n");
		return -1;
	}

	// Setez valoarea flagului la SA_SIGINFO pentru a folosi handler-ul
	// pe care il voi specifica in campul sa_sigaction al variabilei handler_default.
	handler_default.sa_flags = SA_SIGINFO;

	// Initializez masca de biti pentru semnale cu 0, pentru segv_handler.
	rc = sigfillset(&segv_handler.sa_mask);
	// Daca apar probleme, afisez un mesaj de eroare si ies din functie.
	if (rc == -1) {
		printf("Eroare la initializarea mastii de biti pentru segv_handler!\n");
		return -1;
	}

	// Adaug semnalul de SIGSEGV in masca de biti.
	rc = sigaddset(&segv_handler.sa_mask, SIGSEGV);
	// Daca apar probleme, afisez un mesaj de eroare si ies din functie.
	if (rc == -1) {
		printf("Eroare la adaugarea semnalului SIGSEGV in masca de biti!\n");
		return -1;
	}

	// Apelez functia sigaction, avand ca parametrii numele semnalului
	// pe care vreau sa il tratez, handler-ul pentru SIGSEGV si handler-ul default.
	rc = sigaction(SIGSEGV, &segv_handler, &handler_default);
	// Daca apar probleme, afisez un mesaj de eroare si ies din functie.
	if (rc == -1) {
		printf("Eroare la apelul functiei sigaction!\n");
		return -1;
	}

	// Daca nu intampin nicio eroare, se intoarce 0.
	return 0;
}

int so_execute(char *path, char *argv[])
{
	// Se parseaza fisierul executabil.
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	// Deschid fisierul din care voi citi datele.
	file_descriptor = open(path, O_RDONLY);
	// Daca apar probleme, afisez un mesaj de eroare si ies din functie.
	if (file_descriptor == -1) {
		printf("Eroare la deschiderea fisierului!\n");
		return -1;
	}

	// Initializez elementele neinitializate din
	// variabila exec (majoritatea au fost initializate in functia so_parse_exec).
	int i = 0;
	while (i < exec->segments_no) {
		// Aloc spatiu de memorie pentru data.
		exec->segments[i].data = calloc(1, sizeof(struct segment));
		if (exec->segments[i].data == NULL) {
			printf("Eroare la alocarea data!\n");
			return -1;
		}
		// Initial, nu am pagini.
		((struct segment*)(exec->segments[i].data))->numar_pagini = 0;
		((struct segment*)(exec->segments[i].data))->buffer_pagini = NULL;

		i++;
	}

	// Se ruleaza entry point-ul.
	so_start_exec(exec, argv);

	// Daca nu intampin erori, se intoarce 0.
	return 0;
}
