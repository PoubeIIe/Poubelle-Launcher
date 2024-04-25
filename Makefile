launcher:
	g++ launcher.cpp -o main -ljsoncpp -lcurl
	./main

clean:
	rm -rf assets
	rm -rf bin
	rm -rf instances
	rm -rf libraries
	rm -rf version
	rm -rf instances_versions.cfg
	rm -rf version_manifest_v2.json
	clear