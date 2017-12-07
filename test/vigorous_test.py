import os

root = os.path.realpath(__name__)
root = os.path.split(root)[0]

counter = 1
while (100-counter):
    print "<<run number {}>>".format(counter)
    os.system("valgrind --leak-check=full --track-origins=yes "+os.path.join(root, "main"))
    counter += 1
