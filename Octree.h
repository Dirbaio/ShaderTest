#ifndef OCTREE_H
#define OCTREE_H

#include<cstdlib>
#include<vector>


typedef int Cube;
#define OCTREE_SIZE 32

struct SsboOctree
{
		Cube b;
		int children[8];
};

class Octree
{
public:
	Octree() : b(0), children{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}, dirty(false) {}
	Octree(Cube b) : b(b), children{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}, dirty(false) {}

    ~Octree()
    {
        if(!isFilled())
            for(int i = 0; i < 8; i++)
                delete children[i];
    }

    Cube b;
    Octree* children[8];
    bool dirty;

    Cube get(int x, int y, int z, int size) const;
    void set(int x, int y, int z, int size, Cube b2);

	void toSsbo(std::vector<SsboOctree>& v) const;
	inline bool isFilled() const
    {
        return children[0] == NULL;
    }
};

#endif // OCTREE_H
