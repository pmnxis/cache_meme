g++ -g -c ./cache.cpp -o ./cache.o
g++ -g -c ./main.cpp -o ./main.o
g++ -o meme ./cache.o ./main.o
chmod +x ./meme
