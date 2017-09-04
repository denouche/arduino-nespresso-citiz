SHELL := /bin/bash

DIST_FOLDER=dist
VERSION=$(shell cat package.json | grep -Po '"version"\s*:\s*"\K([^"]+)')
SRC_FILE=nespresso.ino

help:
	@echo "Existing goals are: "
	@echo "updateVersion   -> update the version inside $(SRC_FILE) with the version found in package.json"
	@echo "build           -> build the binary in $(DIST_FOLDER)/ folder"

clean:
	rm -rf ./$(DIST_FOLDER)/

updateVersion:
	sed -i -r 's/(const char\* software_version = ")[^"]+(";$$)/\1'$(VERSION)'\2/' $(SRC_FILE)

build: updateVersion
	arduino --board esp8266:esp8266:generic:CpuFrequency=80,UploadSpeed=115200,FlashFreq=40,FlashSize=4M3M --pref build.path=$$PWD/$(DIST_FOLDER) --verify $(SRC_FILE)


