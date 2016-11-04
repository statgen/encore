#ispired by http://code.activestate.com/recipes/577197-sortedcollection/
from bisect import bisect_right, bisect_left
import heapq

class ExtremeCollection(object):
    '''Track only the most extreme values in a collection

    Will keep only the N smallest values
    
    When inserting an element, a displaced element may be returned
    if it is no longer in the extreme tail'''

    def __init__(self, N, iterable=(), key=None):
        self._N = N
        key = ( lambda x: x ) if key is None else key
        self._key = key
        self._items = []
        self._keys = []
        for item in iterable:
            if item is not None:
                self.insert(item)

    def clear(self):
        return self.__class__(self.N, [], self._key)

    def copy(self):
        return self.__class__(self.N, self, self._key)

    def __len__(self):
        return len(self._items)

    def __getitem__(self, i):
        return self._items[i]

    def __iter__(self):
        return iter(self._items)

    def __reversed__(self):
        return reversed(self._items)

    def __contains__(self, item):
        k = self.key(item)
        i = bisect_left(self._keys, k)
        j = bisect_right(self._keys, k)
        return item in self._items[i:j]
    
    def insert(self, item):
        k = self._key(item)
        i = bisect_right(self._keys, k)
        bumped = None
        if i < self._N:
            if len(self._items)==self._N:
                bumped = self._items.pop()
                self._keys.pop()
            self._keys.insert(i, k)
            self._items.insert(i, item)
        else:
            bumped = item
        return bumped

if __name__ == "__main__":
    '''Simple Tests'''
    a = [10,4,9,2,20,16]
    ec1 = ExtremeCollection(3,a)
    adds = [0,10,50,4,4]
    print ", ".join(map(str, ec1)) + " [" + str(len(ec1)) + "]"
    for a in adds:
        print "adding {}".format(a)
        b = ec1.insert(a)
        print ", ".join(map(str, ec1)) + " [" + str(len(ec1)) + "]"
        if b:
            print "bumped: {}".format(b)

