import sys
from heapq import heappush, heappop

from signal import signal, SIGPIPE, SIG_DFL
signal(SIGPIPE,SIG_DFL) 

from collections import Counter

MAX_GENE_DIST = 1000000 

class EventsFileReader():

    def __init__(self, path=None):
        self.path = path

    def __enter__(self):
        if self.path:
            self.f = open(self.path)
        else:
            self.f = sys.stdin
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdin:
            self.f.close()

    def __iter__(self):
        self.itr = iter(self.f)
        return self

    def __next__(self):
        line = next(self.itr)
        chrom, pos, event, gene = line.split()
        pos = int(pos)
        event = int(event)
        return chrom, pos, event, gene

    next = __next__

class BedToEventsReader():

    def __init__(self, path=None):
        self.path = path
        self.has_data = False
        self.done = False
        self.current_chrom = None
        self.lagging_closes = []

    def __enter__(self):
        if self.path:
            self.f = open(self.path)
        else:
            self.f = sys.stdin
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdin:
            self.f.close()
    
    def __readnext(self):
        try:
            line = next(self.itr)
            chrom, start, stop, name = line.split()
            self.last_chrom = chrom
            self.last_start = int(start)
            self.last_stop = int(stop)
            self.last_name = name
            self.has_data = True
        except StopIteration:
            self.done = True
            self.has_data = False

    def __sendcurrent(self):
        heappush(self.lagging_closes, (self.last_stop, self.last_name))
        chrom = self.last_chrom
        pos = self.last_start
        name = self.last_name
        self.__readnext()
        return chrom, pos, 1, name 

    def __sendclose(self):
        cl = heappop(self.lagging_closes)
        return self.last_chrom, cl[0], -1, cl[1]

    def __iter__(self):
        self.itr = iter(self.f)
        try:
            self.__readnext()
            self.current_chrom = self.last_chrom
        except StopIteration:
            pass
        return self

    def __next__(self):
        if self.has_data:
            if len(self.lagging_closes)==0:
                self.current_chrom = self.last_chrom
                return self.__sendcurrent()
            elif self.current_chrom != self.last_chrom:
                return self.__sendclose()
            elif self.last_start < self.lagging_closes[0][0]:
                self.current_chrom = self.last_chrom
                return self.__sendcurrent()
            else:
                return self.__sendclose()
        elif len(self.lagging_closes)>0:
            return self.__sendclose()
        else:
            raise StopIteration

    next = __next__

class BedWriter():
    def __init__(self, path):
        self.path = path;

    def __enter__(self):
        if self.path:
            self.f = open(self.path, "w")
        else:
            self.f = sys.stdout
        return self

    def __exit__(self, type, value, traceback):
        if self.f is not sys.stdout:
            self.f.close()

    def write(self, chrom, start, end, name):
        self.f.write( "\t".join(map(str, [chrom, start, end, name]))) 
        self.f.write("\n")

class PositionCollapser():

    def __init__(self, iterable):
        self.source = iterable
        self.last_chrom = None
        self.last_pos = None
        self.last_gene = None
        self.last_event = None 
        self.done = False
        self.has_data = False

    def __readnext(self):
        try:
            self.has_data = True
            return next(self.itr)
        except StopIteration:
            self.done = True
            self.has_data = False 
            return None, None, None, None

    def __iter__(self):
        self.itr = iter(self.source)
        try:
            self.last_chrom, self.last_pos, self.last_event, self.last_gene = self.__readnext() 
        except:
            pass
        return self

    def __next__(self):
        if self.has_data:
            pos_events = Counter()
            pos_chrom = self.last_chrom
            pos_value = self.last_pos
            pos_events[self.last_gene] += self.last_event
            if not self.done:
                chrom, pos, event, gene = self.__readnext()
                while not self.done and pos == self.last_pos:
                    pos_events[gene] += event
                    chrom, pos, event, gene = self.__readnext()
                self.last_chrom = chrom
                self.last_pos = pos
                self.last_event = event
                self.last_gene = gene
            return pos_chrom, pos_value, pos_events
        else:
            raise StopIteration

    next = __next__

def MakeNearestGeneBED(events, out, max_dist = MAX_GENE_DIST):
    region_pos_open = 0
    last_pos_close = 0
    last_chrom = None
    last_region_name = ""
    open_genes = Counter()

    for chrom, pos, changes in events:
        pre = open_genes
        open_genes = open_genes + changes

        pre_genes = sorted([x for x,c in pre.items()])
        post_genes = sorted([x for x,c in open_genes.items()])

        if last_chrom is not None and chrom != last_chrom:
            if len(pre):
                raise Exception("Chrom " + last_chrom + " ended with open gene region: " + 
                    ",".join([x for (x,c) in pre.items()]))
            close_pos = last_pos_close + max_dist 
            if last_region_name != "":
                out.write(last_chrom, region_pos_open, close_pos, last_region_name) 
            region_pos_open = max(pos - max_dist,0)
            last_chrom = chrom
            continue

        if len(pre_genes)==0 and len(post_genes)>0:
            region_name = ",".join(post_genes)
            dist = pos - last_pos_close
            if dist < 2 * max_dist:
                if last_region_name != region_name and last_region_name != "":
                    close_pos = last_pos_close + dist//2
                    out.write(chrom, region_pos_open, close_pos, last_region_name) 
                    region_pos_open = close_pos + 1
            else:
                close_pos = last_pos_close + max_dist 
                if last_region_name != "":
                    out.write(chrom, region_pos_open, close_pos, last_region_name) 
                region_pos_open = pos - max_dist 
            last_region_name = region_name

        elif len(pre_genes)>0 and len(post_genes)==0:
            last_pos_close = pos
            last_region_name = ",".join(pre_genes)

        else:
            region_name = ",".join(post_genes)
            if last_region_name != region_name:
                out.write(chrom, region_pos_open, pos-1, last_region_name) 
                region_pos_open = pos 
                last_region_name = region_name

        last_chrom = chrom

    #done with iteration. clean up
    if len(open_genes):
        raise Exception("File ended with open gene region: " + ",".join([x for (x,c) in open_genes.items()]))
    if last_region_name != "":
        out.write(chrom, region_pos_open, pos + max_dist, last_region_name)

if __name__ == "__main__":
    infile = sys.argv[1] if len(sys.argv)>1 else None
    outfile = None
    with EventsFileReader(infile) as events, BedWriter(outfile) as out:
        MakeNearestGeneBED(PositionCollapser(events), out)
