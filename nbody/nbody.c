/* File: nbody.c
   Author: Jharrod LaFon
   Date: Spring 2011
   Purpose: Simple N Body simulation
   */

#include<stdio.h>
#include<signal.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<unistd.h>
#include<mpi.h>
#include<math.h>
#include<time.h>
#include "nbody.h"
#include "memwatch.h"
void node_initializer(Node * n,long double x, long double y, long double h, long double w,uint32_t depth)
{
    n->nodeid = 0;
    n->x = x; 
    n->y = y; 
    n->h = h; 
    n->w = w; 
    n->depth = depth;
    n->object = 0x0;
    n->count = 0;
    n->have_children = 0;
    n->have_particle = 0;
    n->ch1 = 0x0;
    n->ch2 = 0x0; 
    n->ch3 = 0x0;
    n->ch4 = 0x0;
    n->cmv = 0x0;;
    return;
}

void print_object(Object * o)
{
    printf("Object [%d] Pos(x,y) (%LG,%LG) V(x,y): (%LG,%LG) F(x,y): (%LG,%LG) M: %LG \n",o->id,o->x,o->y,o->vx,o->vy,o->force.x,o->force.y,o->m);
}
void print_node(Node * n)
{
    printf("Node (%LG => %LG,%LG => %LG) Depth: %d Count %d Has Particle: %s\n",n->x,n->x+n->w,n->y,n->y+n->h,n->depth,n->count,(n->have_particle)?"true":"false");
}

long double distance(Object * p1, Object * p2)
{
    return sqrt(pow(p2->x - p1->x,2.0)+pow(p2->y - p1->y,2.0));
}

long double distanceN(Object * p1, Node * p2)
{
    long double v = sqrt(pow(p2->cmv->x - p1->x,2.0)+pow(p2->cmv->y - p1->y,2.0));
    return v;
}

void force_vectorN(Vector* v,Object * p1, Node * p2)
{
    long double dist = distanceN(p1,p2);
    if(dist < ACCURACY)
        dist += ACCURACY;
    long double f = (G*p1->m*p2->cmv->mass)/(pow(dist,2.0));
    v->x = v->x + f*((p2->cmv->x - p1->x)/dist);
    v->y = v->y + f*((p2->cmv->y - p1->y)/dist);
    return;// *v;
}

void force_vector(Vector *v,Object * p1, Object * p2)
{
    long double dist = distance(p1,p2);
    if(dist < ACCURACY)
        dist+= ACCURACY;

    long double f = (G*p1->m*p2->m)/(pow(dist,2.0));
    long double a = ((p2->x - p1->x)/dist);
    v->x = v->x + f*a;
    long double b = ((p2->y - p1->y)/dist);
    v->y = v->y + f*b;
    return;// *v;
}


int insert_in_quadrant(Object * ob, Node * root)
{
   uint32_t depth = 0;
    if(ob->x < root->ch2->x)
    {
        if(ob->y < root->ch3->y)
        {
            depth += insert((Object *)ob,(Node*)root->ch1);
        }
        else if(ob->y >= root->ch3->y)
        {
            depth += insert((Object *)ob,(Node*)root->ch3);
        }
        else
        {
            printf("Bounds error [%d] (%LG,%LG) (%LG:%LG,%LG:%LG)\n",__LINE__,ob->x,ob->y, root->ch3->x,root->ch3->x+root->ch3->w,root->ch3->y,root->ch3->h);
           // print_node(root->ch4);
            print_object(ob);
            exit(1);
        }
    }
    else if(ob->x >= root->ch2->x)
    {
        if(ob->y < root->ch4->y)
        {
            depth += insert((Object *)ob,(Node*)root->ch2);
        }
        else if(ob->y >= root->ch4->y)
        {
            depth += insert((Object *)ob,(Node*)root->ch4);
        }
        else
        {
            printf("Bounds error [%d] %LG %LG\n", __LINE__,ob->y, root->ch4->y);
           // print_node(root->ch4);
            print_object(ob);
            exit(1);
        }
    }
    else
     {
            printf("Bounds error [%d] %LG %LG\n", __LINE__,ob->y, ob->x);
           // print_node(root->ch4);
            print_object(ob);
            exit(1);
        }
return depth;
}
void fix_collision(Object * o1, Object * o2)
{
    o1->x = o1->x + (rand()%100+1)/100;
    if(o1->x > 800)
        o1->x = o1->x - 800;
}
void check_collisions(Object * obs, int num)
{
    int i,j;
    for(i = 0; i < num; i++)
        for(j = 0; j < num; j++)
            if(fabs(obs[i].x - obs[j].x) < ACCURACY && fabs(obs[i].y - obs[j].y) < ACCURACY)
                fix_collision(&obs[i],&obs[j]);
}
int insert(Object * ob, Node * root)
{
    uint32_t depth = 1;
    root->count = root->count+1;
    /* Subtree rooted at n is empty */
    if(root->count == 1)
    {
        root->object = ob;
        root->have_particle = 1;
        goto idone;
    }
    else if(root->count > 2)
    {
        insert_in_quadrant((Object *)ob,(Node*)root);
        goto idone;
    }
    else if(root->count == 2)
    {
        root->have_children = 1;
        Node * n1 = malloc(sizeof(Node));
        Node * n2 = malloc(sizeof(Node));
        Node * n3 = malloc(sizeof(Node));
        Node * n4 = malloc(sizeof(Node));
        node_initializer((Node *) n1, root->x,root->y,root->h/2.0,root->w/2.0,root->depth+1);
        node_initializer((Node *) n2, root->x+root->w/2.0,root->y,root->h/2.0,root->w/2.0,root->depth+1);
        node_initializer((Node *) n3, root->x,root->y+root->h/2.0,root->h/2.0,root->w/2.0,root->depth+1);
        node_initializer((Node *) n4, root->x+root->w/2.0,root->y+root->h/2.0,root->h/2.0,root->w/2.0,root->depth+1);
        root->ch1 = (Node *)n1;
        root->ch2 = (Node *)n2;
        root->ch3 = (Node *)n3;
        root->ch4 = (Node *)n4;
        if(fabs(root->object->x - ob->x) < ACCURACY && fabs(root->object->y - ob->y) < ACCURACY)
        {
            root->ch1->object = ob;
            root->ch1->have_particle = 1;
            root->ch2->count = 1;
            root->have_particle = 0;
            root->ch2->object = root->object;
            root->ch2->have_particle = 1;
            root->ch2->count = 1;
            goto idone;
        }
        else
        {
            insert_in_quadrant((Object *)ob,(Node*)root);
            insert_in_quadrant((Object *)root->object,(Node *)root);
        }
    }
idone:   
    return depth;
}


void build_quad_tree(Node * root,Object * objects,int d)
{
   uint32_t i;
   uint32_t j;
    
   //print_object(&objects[0]);
    for(i = 0; i < d; i++)
    {
        j = insert((Object*)&objects[i],root);
    }
}

void print_nodes(Object * p,uint32_t num_objects)
{
   uint32_t i = 0;
    for(i = 0; i < num_objects; i++)
    {
        printf("Object[%d] (%3.10Lf,%3.10Lf) F(%3.10Lf,%3.10Lf)\n",p[i].id,p[i].x,p[i].y,p[i].force.x,p[i].force.y);
    }
    return;
}




CMVector * compute_center_mass(Node * root)
{
    CMVector * v = (CMVector *) calloc(1,sizeof(CMVector));
    v->x = v->y = v->mass = 0;
    root->cmv = v;
    /* if n contains 1 particle */
    if(!root->have_children && root->have_particle)
    {
        root->cmv->x = root->object->x;
        root->cmv->y = root->object->y;
        root->cmv->mass = root->object->m;
    }
    else if(root->have_children)
    {
        CMVector * cms1;
        CMVector * cms2;
        CMVector * cms3;
        CMVector * cms4;
        cms1 = compute_center_mass(root->ch1);
        cms2 = compute_center_mass(root->ch2);
        cms3 = compute_center_mass(root->ch3);
        cms4 = compute_center_mass(root->ch4);
        v->mass = cms1->mass + cms2->mass + cms3->mass + cms4->mass;
        v->x = (cms1->mass*cms1->x + cms2->mass*cms2->x + cms3->mass*cms3->x + cms4->mass*cms4->x)/v->mass;
        v->y = (cms1->mass*cms1->y + cms2->mass*cms2->y + cms3->mass*cms3->y + cms4->mass*cms4->y)/v->mass;
        root->cmv->mass = v->mass;
        root->cmv->x = v->x;
        root->cmv->mass = v->mass;
    }

   return v;    
}
void compute_force(Vector * f,Object * ob, Node * root)
{
    long double dist = 0.0;
    long double D = root->w;
    long double theta = 1.0;
    if(root->count == 0)
    {
        return ;
    }

    else if(root->count == 1)
    {
	force_vector((Vector *)f,(Object*)ob,(Object*)root->object);
    }
    else if(root->count > 1)
    {
       dist = distanceN((Object*)ob,(Node*)root);
       if(D/dist < theta)
       {
	   force_vectorN((Vector*)f,(Object*)ob,(Node*)root);
       }
       else
       {
	   compute_force((Vector*)f,(Object*)ob,(Node*)root->ch1);
       compute_force((Vector*)f,(Object*)ob,(Node*)root->ch2);
       compute_force((Vector*)f,(Object*)ob,(Node*)root->ch3);
       compute_force((Vector*)f,(Object*)ob,(Node*)root->ch4);
       }
    }
    return;
}
void compute_forces(Node * root,Params *p)
{
   uint32_t i;
    Vector temp;
    for(i = 0; i < p->num_objects; i++)
    {
        p->objects[i].force.x = temp.x;
        p->objects[i].force.y = temp.y;
    }
}

void update(Object * obs,uint32_t base,int num_objects, long double dt)
{
   uint32_t i;
    for(i = base; i < base+num_objects; i++)
    {
        obs[i].vx = obs[i].vx + obs[i].force.x*dt/obs[i].m;
        obs[i].vy = obs[i].vy + obs[i].force.y*dt/obs[i].m;
        obs[i].x = obs[i].x + obs[i].vx*dt;
        obs[i].y = obs[i].y + obs[i].vy*dt;
        while(obs[i].y > 800)
            obs[i].y = obs[i].y - 800;
        while(obs[i].y < 0)
            obs[i].y = obs[i].y + 800;
        while(obs[i].x > 800)
            obs[i].x = obs[i].x - 800;
        while(obs[i].x < 0)
            obs[i].x = obs[i].x + 800;
    }
}
void purge_tree(Node * t)
{
    /* Base Case, but don't purge root */
    if(t->have_children)
    {
        purge_tree(t->ch1);
        purge_tree(t->ch2);
        purge_tree(t->ch3);
        purge_tree(t->ch4);
    }
    else
    {
	if(t->depth != 0)
    {
        free(t->cmv);
	    free(t);
    }
    }
}
void print_compute_node_tree(ComputeNode *cn)
{
   uint32_t i;
    for(i = 0; i < cn->depth; i++)
	printf("\t");
    printf("Tree Quadrant (%LG -> %LG), (%LG -> %LG) Rank: %d\n",cn->xmin,cn->xmin+cn->w,cn->ymin,cn->ymin+cn->h,cn->rank);
    if(cn->have_children)
    {
	print_compute_node_tree(cn->ch1);
	print_compute_node_tree(cn->ch2);
	print_compute_node_tree(cn->ch3);
	print_compute_node_tree(cn->ch4);
    }
}
void build_compute_node_tree(ComputeNode * cn,uint32_t min,int size)
{
    ComputeNode * c1 = (ComputeNode *) malloc(sizeof(ComputeNode));
    ComputeNode * c2 = (ComputeNode *) malloc(sizeof(ComputeNode));
    ComputeNode * c3 = (ComputeNode *) malloc(sizeof(ComputeNode));
    ComputeNode * c4 = (ComputeNode *) malloc(sizeof(ComputeNode));
    ComputeNode_initializer((ComputeNode *) c1, cn->xmin,cn->ymin,cn->h/2.0,cn->w/2.0,cn->depth+1,min+size/4*0);
    ComputeNode_initializer((ComputeNode *) c2, cn->xmin+cn->w/2.0,cn->ymin,cn->h/2.0,cn->w/2.0,cn->depth+1,min+size/4*1);
    ComputeNode_initializer((ComputeNode *) c3, cn->xmin,cn->ymin+cn->h/2.0,cn->h/2.0,cn->w/2.0,cn->depth+1,min+size/4*2);
    ComputeNode_initializer((ComputeNode *) c4, cn->xmin+cn->w/2.0,cn->ymin+cn->h/2.0,cn->h/2.0,cn->w/2.0,cn->depth+1,min+size/4*3);
    cn->ch1 = c1;
    cn->ch2 = c2;
    cn->ch3 = c3;
    cn->ch4 = c4;
    cn->have_children = 1;
    if(size > 4)
    {
	build_compute_node_tree((ComputeNode *) cn->ch1,min+size/4*0,size/4);
	build_compute_node_tree((ComputeNode *) cn->ch2,min+size/4*1,size/4);
	build_compute_node_tree((ComputeNode *) cn->ch3,min+size/4*2,size/4);
	build_compute_node_tree((ComputeNode *) cn->ch4,min+size/4*3,size/4);
    }
    return;
}
/* Function to be executed by the workers */
void slave(Params * p)
{ 
    /* Timer */
    long double start, elapsed,end,average_comm,temp_comm,average_iteration,temp_build_quad_tree, average_build_quad_tree, temp_compute_center_mass, average_compute_center_mass;
    Node root;
   uint32_t j;
   uint32_t i;
   int * sizes = (int *)malloc(sizeof(int)*p->size);
   int * disp = (int *)malloc(sizeof(int)*p->size);
    
    /* Calculate my block size */
   uint32_t size =  BLOCK_SIZE(p->rank,p->size,p->num_objects)*sizeof(Object);
    /* Gather sizes once, they don't change */
    MPI_Gather(&size,1,MPI_INT,sizes,1,MPI_INT,0,MPI_COMM_WORLD);
    /* Distribute sizes to all ranks */
    MPI_Bcast(sizes,p->size,MPI_INT,0,MPI_COMM_WORLD);
    
    /* Now that all ranks have the block sizes, calculate the array displacements */
    disp[0] = 0;
    for(j = 1; j < p->size; j++)
        disp[j] = disp[j-1] + sizes[j-1];
   // ComputeNode compute_root;
 //   ComputeNode_initializer(&compute_root,0.0,0.0,p->uy,p->ux,0,0);
//    build_compute_node_tree(&compute_root,0,p->size);
//    if(p->rank == 0)print_compute_node_tree(&compute_root);
    
    /* Array to hold all objects */
    Object * objects = (Object *) malloc(sizeof(Object)*p->num_objects);
    /* Array to hold my objects */
    Object * send_buf = (Object *) malloc(sizeof(Object)*size);
    
    /* Initialize Objects */
    if(p->rank == 0)
    {
        /* Seed random number generator */
        srand(10);
        for(i = 0; i < p->num_objects; i++)
        {
	    /* Generate randomuint32_t from 0 -> p->ux or p->y + random long double from 0 -> 1 */
            objects[i].x = (long double) ((rand()%(int)p->ux-1)) + ((long double)rand()/(long double)RAND_MAX);
            objects[i].y = (long double) ((rand()%(int)p->uy-1)) + ((long double)rand()/(long double)RAND_MAX);
            objects[i].m = (long double) ((rand()%(int)p->max_mass+1)) + ((long double)rand()/(long double)RAND_MAX)+2;
            objects[i].id = i;
            objects[i].force.x = 0.0;
            objects[i].force.y = 0.0;
            objects[i].vx = 0;
            objects[i].vy = 0;
        }
    }

    /* Broad cast objects to all ranks */
    MPI_Bcast(objects,p->num_objects*sizeof(Object),MPI_BYTE,0,MPI_COMM_WORLD);
    p->objects = objects;
    
    /* Timing variables */
    average_iteration = 0.0;
    average_build_quad_tree = 0.0;
    average_compute_center_mass = 0.0;
    average_comm = 0.0;
    FILE * log;
    /* Start the clock */
    elapsed = MPI_Wtime();
    if(p->rank == 0)
    {
        log = fopen("nbody.xyz","w+");
    }
    /* Main loop of simulation */
    for(i = 0; i < p->max_iterations; i++)
    {
        /* Each iteration is timed */
        start = MPI_Wtime();

        /* Initialize root */
        node_initializer((Node *) &root,0.0, 0.0, p->ux, p->uy, 0);
        state = BUILD_QUAD_TREE;        
        /* Build the quad tree */
        temp_build_quad_tree = MPI_Wtime();
        build_quad_tree((Node *)&root,(Object *)objects,p->num_objects);
        average_build_quad_tree += MPI_Wtime() - temp_build_quad_tree;
        /* Determine objects center masses */
        temp_compute_center_mass = MPI_Wtime();
        compute_center_mass(&root);
        average_compute_center_mass += MPI_Wtime() - temp_compute_center_mass;
           
        /* Compute forces.  Each rank computes the forces on it's block of objects only */
       uint32_t k,l = 0;
        for(k = BLOCK_LOW(p->rank,p->size,p->num_objects); k < BLOCK_HIGH(p->rank,p->size,p->num_objects); k++)
        {
	    /* Copy to a temporary buffer */
            memcpy(&send_buf[l],&p->objects[k],sizeof(Object));
            send_buf[l].force.x = 0.0;
            send_buf[l].force.y = 0.0;
	    /* Compute the force on the object */
            compute_force((Vector*)&send_buf[l].force,(Object*)&send_buf[l],(Node*)&root);
	    l++;
        }
        /* Update the positions of the particles in my block */
        update(send_buf,0,size/sizeof(Object),p->time_step);
        
        /* Exchange blocks */
        temp_comm = MPI_Wtime();
        MPI_Allgatherv(send_buf,size,MPI_BYTE,objects,sizes,disp,MPI_BYTE,MPI_COMM_WORLD);
        if(p->rank == 0)
        {   
            fprintf(log, "%d\n",p->num_objects);
            fprintf(log, "N-Body Simulation\n");
            for(k = 0; k < p->num_objects; k++)
            {
                fprintf(log,"\tP\t%LG\t%LG\t1.0\n",objects[k].x,objects[k].y);
            }
            fprintf(log, "\n");
            fflush(log);
    }

        average_comm += MPI_Wtime() - temp_comm;
        purge_tree(&root);
        end = MPI_Wtime();
        average_iteration += end-start;
    } 
    elapsed = end-elapsed;
    average_iteration = average_iteration / (i+1);
    average_compute_center_mass = average_compute_center_mass/(i+1);
    average_comm = average_comm/(i+1);
    average_build_quad_tree = average_build_quad_tree/(i+1);
    if(p->rank == 0) 
    {
        fclose(log);
        printf("Total time: %LG s Average Iteration Time %LG s Average Build Quad Tree %LG s Average Compute Center Mass %LG s Average Comm: %LG s\n",elapsed,average_iteration,average_build_quad_tree/(i+1),average_compute_center_mass,average_comm);
        printf("Build Quad tree %LG %% Compute Center Mass %LG %% Comm: %LG %%\n",100.0*average_build_quad_tree/average_iteration,100.0*average_compute_center_mass/average_iteration,100.0*average_comm/average_iteration);
    }
    return;

}

void usage()
{
    printf("bucket_sort\n\
            MPI Program to sort an array using bucket sort.\n\
            Jharrod LaFon 2011\n\
            Usage: bucket_sort [args]\n\
            h\t\tPrint this message\n");
}
/* Parse user arguments */
void parse_args(int argc, char ** argv, Params * p)
{
   uint32_t c = 0;
    while((c = getopt(argc,argv,"s:i:t:m:")) != -1)
    {
        switch(c)
        {
            case 's':
                p->num_objects = atoi(optarg);
                break;
            case 'i':
                p->max_iterations = atoi(optarg);
                break;
            case 't':
                p->time_step = atof(optarg);
                break;
            case 'm':
                p->max_mass = atoi(optarg);
                break;
            case 'h':
                exit(0);
                break;
	    default:
                break;
        }
    }
    return;
}

/* Main */
int main(int argc, char *argv[])
{
   int rank;
   int size;
    /* Initialize MPI */
    if(MPI_Init(&argc, &argv) != MPI_SUCCESS)
    {
        printf( "Unable to initialize MPI!\n");
        return -1;
    }
    /* Get rank and size */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    /* Populate the parameters with default values */
    Params p;
    p.rank = rank;
    p.size = size;
    p.ux = UNIVERSE_X;
    p.uy = UNIVERSE_Y;
    p.max_mass = MAX_MASS;
    p.max_iterations = MAX_ITERATIONS;
    p.time_step = TIME_STEP;
    p.num_objects = NUM_OBJECTS;
    /* Don't be verbose by default */
    parse_args(argc,argv,&p);
    slave((Params *)&p);
    MPI_Finalize();
    return 0;
}
