PIO ?= pio
BAUD ?= 9600

.DEFAULT_GOAL := help

.PHONY: help build upload monitor deploy

help:
	@echo "Arduino Uno commands:"
	@echo "  make build                 Compile without uploading"
	@echo "  make upload                Compile and upload"
	@echo "  make monitor               Open serial monitor at 9600 baud"
	@echo "  make deploy                Compile, upload, then monitor"
	@echo "  make deploy BAUD=115200    Override the monitor baud rate"

build:
	$(PIO) run

upload:
	$(PIO) run --target upload

monitor:
	$(PIO) device monitor --baud $(BAUD)

deploy:
	$(PIO) run --target upload
	$(PIO) device monitor --baud $(BAUD)
