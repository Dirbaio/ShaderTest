#include "Octree.h"

Cube Octree::get(int x, int y, int z, int size) const
{
    if(isFilled()) return b;

    int halfsize = size-1;
    int xx = (x >> halfsize) & 1;
    int yy = (y >> halfsize) & 1;
    int zz = (z >> halfsize) & 1;
    int ind = xx | (yy<<1) | (zz<<2);

    return children[ind]->get(x, y, z, size-1);
}

void Octree::set(int x, int y, int z, int size, Cube b2)
{
    if(isFilled() && b == b2)
        return;

    if(size == 0)
    {
        if(b != b2)
        {
            b = b2;
            dirty = true;
        }
        return;
    }

    //Subdividir if needed.
    if(isFilled())
        for(int i = 0; i < 8; i++)
            children[i] = new Octree(b);

    //Setear el hijo
    int halfsize = size-1;
    int xx = (x >> halfsize) & 1;
    int yy = (y >> halfsize) & 1;
    int zz = (z >> halfsize) & 1;
    int ind = xx | (yy<<1) | (zz<<2);

    children[ind]->set(x, y, z, size-1, b2);
    dirty |= children[ind]->dirty;

    //Juntar if needed.
    bool same = children[0]->isFilled();
    for(int i = 1; i < 8 && same; i++)
        if(!children[i]->isFilled() || children[i]->b != children[0]->b)
            same = false;

    if(same)
    {
        b = children[0]->b;
        for(int i = 0; i < 8; i++)
        {
            delete children[i];
			children[i] = NULL;
        }
    }
}

void Octree::toSsbo(std::vector<SsboOctree>& v) const
{
	SsboOctree o;
	o.b = this->b;

	int ind = v.size();
	v.push_back(o);

	for(int i = 0; i < 8; i++)
	{
		if(children[i])
		{
			v[ind].children[i] = v.size();
			children[i]->toSsbo(v);
		}
		else
			v[ind].children[i] = 0;
	}
}
