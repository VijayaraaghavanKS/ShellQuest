all: shellquest shellquest_gui

shellquest: shellquest.c
	gcc -o shellquest shellquest.c -lreadline

shellquest_gui: shellquest_gui.c
	gcc -o shellquest_gui shellquest_gui.c `pkg-config --cflags --libs gtk+-3.0 vte-2.91`

install:
	sudo cp shellquest /usr/local/bin/
	sudo cp shellquest_gui /usr/local/bin/
	sudo chmod +x /usr/local/bin/shellquest /usr/local/bin/shellquest_gui

clean:
	rm -f shellquest shellquest_gui
