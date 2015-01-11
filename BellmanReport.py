import sys, os, glob
import subprocess
import golly as g 
from glife.text import make_text


def CreateRule():
   fname = os.path.join(g.getdir("rules"), "LifeBellman.rule")
   
   with open(fname, 'wb') as f:
		f.write('@RULE LifeBellman\n')
		f.write('\n')
		f.write('@TABLE\n')
		f.write('\n')
		f.write('# state 0:  OFF\n')
		f.write('# state 1:  ON\n')
		f.write('# state 2:  History ON now OFF\n')
		f.write('# state 3:  Catalyst ON \n')
		f.write('# state 4:  Catalyst OFF\n')
		f.write('# state 5:  Catalyst UNKNOWN\n')
		f.write('\n')
		f.write('n_states:6\n')
		f.write('neighborhood:Moore\n')
		f.write('symmetries:rotate8\n')
		f.write('\n')
		f.write('var a={0,2,4,5}\n')
		f.write('var b={0,2,4,5}\n')
		f.write('var c={0,2,4,5}\n')
		f.write('var d={0,2,4,5}\n')
		f.write('var e={0,2,4,5}\n')
		f.write('var f={0,2,4,5}\n')
		f.write('var g={3}\n')
		f.write('var h={0,1,2,5}\n')
		f.write('var i={0,1,2,3,4,5}\n')
		f.write('var j={0,1,2,3,4,5}\n')
		f.write('var k={0,1,2,3,4,5}\n')
		f.write('var l={0,1,2,3,4,5}\n')
		f.write('var m={0,1,2,3,4,5}\n')
		f.write('var n={0,1,2,3,4,5}\n')
		f.write('var o={0,1,2,3,4,5}\n')
		f.write('var p={0,1,2,3,4,5}\n')
		f.write('\n')
		f.write('var i1={0,1,2,3,4,5}\n')
		f.write('var j1={0,1,2,3,4,5}\n')
		f.write('var k1={0,1,2,3,4,5}\n')
		f.write('var l1={0,1,2,3,4,5}\n')
		f.write('var m1={0,1,2,3,4,5}\n')
		f.write('var n1={0,1,2,3,4,5}\n')
		f.write('var o1={0,1,2,3,4,5}\n')
		f.write('var p1={0,1,2,3,4,5}\n')
		f.write('\n')
		f.write('var q={1,3}\n')
		f.write('var R={1,3}\n')
		f.write('var S={1,3}\n')
		f.write('var T={1,3}\n')
		f.write('var u={3,4}\n')
		f.write('\n')
		f.write('# Catalyst-doesn\'t change with unknown neighbour\n')
		f.write('3,5,i,j,k,l,m,n,o,3\n')
		f.write('5,3,i1,j1,k1,l1,m1,n1,o1,5\n')
		f.write('0,5,3,j1,k1,l1,m1,n1,o1,0\n')
		f.write('0,5,j1,3,k1,l1,m1,n1,o1,0\n')
		f.write('0,5,j1,k1,3,l1,m1,n1,o1,0\n')
		f.write('0,5,j1,k1,l1,3,m1,n1,o1,0\n')
		f.write('0,5,j1,k1,l1,m1,3,n1,o1,0\n')
		f.write('\n')
		f.write('# Catalyst-neighbour birth\n')
		f.write('4,R,S,T,a,b,c,d,e,3\n')
		f.write('4,R,S,a,T,b,c,d,e,3\n')
		f.write('4,R,S,a,b,T,c,d,e,3\n')
		f.write('4,R,S,a,b,c,T,d,e,3\n')
		f.write('4,R,S,a,b,c,d,T,e,3\n')
		f.write('4,R,a,S,b,T,c,d,e,3\n')
		f.write('4,R,a,S,b,c,T,d,e,3\n')
		f.write('\n')
		f.write('# Catalyst-neighbour survival\n')
		f.write('g,R,S,T,a,b,c,d,e,g\n')
		f.write('g,R,S,a,T,b,c,d,e,g\n')
		f.write('g,R,S,a,b,T,c,d,e,g\n')
		f.write('g,R,S,a,b,c,T,d,e,g\n')
		f.write('g,R,S,a,b,c,d,T,e,g\n')
		f.write('g,R,a,S,b,T,c,d,e,g\n')
		f.write('g,R,a,S,b,c,T,d,e,g\n')
		f.write('\n')
		f.write('# normal 3-neighbour birth\n')
		f.write('h,R,S,T,a,b,c,d,e,1\n')
		f.write('h,R,S,a,T,b,c,d,e,1\n')
		f.write('h,R,S,a,b,T,c,d,e,1\n')
		f.write('h,R,S,a,b,c,T,d,e,1\n')
		f.write('h,R,S,a,b,c,d,T,e,1\n')
		f.write('h,R,a,S,b,T,c,d,e,1\n')
		f.write('h,R,a,S,b,c,T,d,e,1\n')
		f.write('\n')
		f.write('# 2-neighbour survival\n')
		f.write('q,R,S,a,b,c,d,e,f,q\n')
		f.write('q,R,a,S,b,c,d,e,f,q\n')
		f.write('q,R,a,b,S,c,d,e,f,q\n')
		f.write('q,R,a,b,c,S,d,e,f,q\n')
		f.write('\n')
		f.write('# ON states 3 and 5 go to history state 4 if they don\'t survive\n')
		f.write('g,i,j,k,l,m,n,o,p,4\n')
		f.write('\n')
		f.write('# Otherwise ON states die and become the history state\n')
		f.write('q,i,j,k,l,m,n,o,p,2\n')
		f.write('\n')
		f.write('@COLORS\n')
		f.write('\n')
		f.write('0    48  48    48\n')
		f.write('1    255  255    255\n')
		f.write('2    0    0  0\n')
		f.write('3  0  191  225\n')
		f.write('4  255    0    0\n')
		f.write('5  120  255    0\n')
		
def AddText(val, x, y):
	t = make_text(val, "mono")
	newt = []
	cnt = 0 

	for i in t:
	   cnt += 1
	   newt.append(i)
	   
	   if cnt % 2 == 0:
		  newt.append(4)
		  
	if len(newt) % 2 == 0:
	   newt.append(0)
	   
	g.putcells(newt, x, y)


def putcells(outfile, dxy):
	dx, dy = dxy
	tileCount = -1
	maxTileX = -1
	maxTileY = -1

	with open(outfile, "r") as data:
		for line in data:
			
			if tileCount >= 0:
				
				chars = list(line)
				
				for x in xrange(0, len(chars)):
					ch = chars[x]
					
					if ch == '@':
						g.setcell(dx + tileDX + x, dy + tileCount + tileDY, 1)
					if ch == '*':
						g.setcell(dx + tileDX + x, dy + tileCount + tileDY, 3)
					if ch == '?':
						g.setcell(dx + tileDX + x, dy + tileCount + tileDY, 5)
					
						
				tileCount += 1
			
			if line.startswith("#S"):
				continue 
			
			if line.startswith("#P"):
				vals = line.split()
				tileDX = int(vals[1])
				tileDY = int(vals[2])
				tileCount = 0
				
				if tileDX > maxTileX:
					maxTileX = tileDX
				
				if tileDY > maxTileY:
					maxTileY = tileDY
				
	return (maxTileX, maxTileY)

class Category:
    def __init__(self, category):
        self.category = category
        self.results = []
 
def FillCategories(path):
	
	files = glob.glob(os.path.join(path, "*.out"))
	g.show("Loading {0} files, please  wait...".format(str(len(files))))

	cats = {}
	idx = 0
	
	for f in files:
		
		lines = subprocess.Popen([os.path.join(os.path.dirname(os.path.realpath('__file__')), "bellman.exe"), "-c", f], stdout=subprocess.PIPE,  shell=True).communicate()[0]
		lines = lines.split("\n")
		d = {}
		for l in lines:
			idx = l.find(":")
			if idx > 0:
				key = l[:idx].strip()
				val = l[idx+1:].strip()
				if d.has_key(key):
					prev = d[key] + "\n"
				else:
					prev = ""
				d[key] = prev + val
		category = d["hash"]
		
		if not cats.has_key(category):
			cats[category] = Category(category)
			
		cats[category].results.append((f, d["log"]))
		
		
	return cats

def Place(path):
    
	cats = FillCategories(path)
	keysL = sorted(cats.keys(), key=lambda x: cats[x].results[0][0])
	g.show("Found {0} Categories".format(len(keysL)))
	
	dx, dy = 0, 0
	
	for c in keysL:
		results = cats[c].results
		AddText(str(len(results)), dx - 30, dy + 10)
		
		for i in xrange(0, len(results)):
			fname = os.path.splitext(os.path.basename(results[i][0]))[0]
			fname = fname.replace("result", "").lstrip("0").replace("-4", "")
			
			mdx, mdy = putcells(results[i][0], (dx, dy))
			
			AddText(fname, dx + int(mdx / 2), dy - 10)
			
			dx += mdx + 60
				
		dy += 60 + mdy
		dx = 0
	
	AddText(path, 0, -100)

#Start Here 
CreateRule()
g.setrule("LifeBellman")

file_path = os.path.join(g.getdir("data"), "LastBellmanOutputDir.txt")
dir = ""

if os.path.exists(file_path):
	with open (file_path, "r") as f:
		dir=f.readline()

dir = g.getstring("Enter output directory", dir) 

with open(file_path, "w") as text_file:
    text_file.write(dir)

Place(dir)
