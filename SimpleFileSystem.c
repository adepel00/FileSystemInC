#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Needed for the macros */
#include <linux/pagemap.h>   /* PAGE_CACHE_SIZE */
#include <linux/fs.h>        /* libfs stuff */
#include <asm/atomic.h>      /* atomic_t stuff */
#include <asm/uaccess.h>     /* copy_to_user */
#include <linux/slab.h>     /*De la reserva dinámica*/
#define TMPSIZE 100
#define LFS_MAGIC 0x19980122

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alba de Pedro López");

//Contadores
static atomic_t contador1, contador2;

char *texto;
int posicion = 0;

//Declaración de funciones
static int __init assoofs_init(void);
static struct dentry *assoofs_get_super(struct file_system_type *fst, int flags, const char *devname , void *data);
static int assoofs_fill_super(struct super_block *sb, void *data, int silent);
static struct dentry *assoofs_create_dir (struct super_block *sb, struct dentry *parent, const char *name);
static void assoofs_create_files(struct super_block *sb, struct dentry *root);
static struct dentry *assoofs_create_file(struct super_block *sb , struct dentry *dir, const char *name , atomic_t *counter);
static struct inode *assoofs_make_inode(struct super_block *sb, int mode);
static int assoofs_open (struct inode *inode, struct file *filp);
static ssize_t assoofs_read_file( struct file *filp , char *buf, size_t count, loff_t *offset);
static ssize_t assoofs_write_file( struct file *filp, const char *buf, size_t count, loff_t *offset);
static void __exit assoofs_exit(void);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);

//Estructura que representa el tipo del sistema de ficheros
static struct file_system_type assoofs_type = {
	.owner = THIS_MODULE,
	.name = "assoofs",
	.mount = assoofs_get_super,
	.kill_sb = kill_litter_super,
};

//Estructura con las operaciones con inodos

static struct inode_operations assoofs_inode_ops = {
    .create = assoofs_create,
    .lookup = assoofs_lookup,
    .mkdir = assoofs_mkdir,
};

//Estructura de las operaciones con ficheros
static struct file_operations assoofs_file_ops = {
	.open = assoofs_open,
	.read = assoofs_read_file,
	.write = assoofs_write_file,
};

/*
*Estructura que define las operaciones que se pueden realizar
*con el superbloque
*/
static struct super_operations assoofs_s_ops = {
	.statfs = simple_statfs,
	.drop_inode = generic_delete_inode,
};


//Inicialización del sistema de ficheros
static int __init assoofs_init(void){
	return register_filesystem(&assoofs_type);
}

//Se ejecuta cuando se registra el sistema de ficheros
static struct dentry *assoofs_get_super(struct file_system_type *fst, int flags, const char *devname , void *data){
	return mount_bdev(fst, flags, devname, data, assoofs_fill_super);
}

//Función que se encarga de configurar el sistema de ficheros
static int assoofs_fill_super(struct super_block *sb, void *data, int silent){
	struct inode *root;
	struct dentry *root_dentry;
	//Configuraciones básicas del superbloque
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = LFS_MAGIC;
	sb->s_op = &assoofs_s_ops;
	
	//Creamos el nodo raiz
	root = assoofs_make_inode(sb, S_IFDIR | 0777 ); //0755
	if(!root){
		printk("Ha fallado la creación de la raíz");
		return -EFAULT;
	}
	root->i_op = &assoofs_inode_ops;
	root->i_fop = &simple_dir_operations;

	root_dentry = d_make_root(root);
	if(!root_dentry){
		printk("Ha fallado la creación la entrada al directorio de la raíz");
		return -EFAULT;
	}
	sb->s_root = root_dentry;
	assoofs_create_files (sb, root_dentry);
	return 0;
}

/*
* Función que crea directorios, como crear ficheros pero con las
* operaciones de la raíz
*/
static struct dentry *assoofs_create_dir (struct super_block *sb, struct dentry *parent, const char *name){
	struct dentry *dentry;
	struct inode *inode;
	struct qstr qname;

	qname.name = name;
	qname.len = strlen(name);
	qname.hash = full_name_hash(name, qname.len);
	
	dentry = d_alloc(parent, &qname);
	
	if (!dentry){
		printk(KERN_ERR "Ha fallado la creación del directorio");
		return NULL;
	}
	inode = assoofs_make_inode(sb, S_IFDIR | 0777); //0755
	if (!inode){
		printk(KERN_ERR "Ha fallado la creación la entrada al directorio");
		return NULL;
	}
	inode->i_op = &assoofs_inode_ops; 
	inode->i_fop = &simple_dir_operations;

	d_add(dentry, inode);
	return dentry;
}

//Función que crea los ficheros que necesito
static void assoofs_create_files(struct super_block *sb, struct dentry *root){
	struct dentry *carpeta1;
	
	texto = (char *) kmalloc ((sizeof(char) *TMPSIZE)*30, GFP_KERNEL);
	
	//Un contador1 para el directorio raíz
	atomic_set(&contador1, 0); //Inicializo
	assoofs_create_file(sb , root , "contador1" , &contador1);
	
	//Un contador2 para el subdirectorio
	atomic_set(&contador2, 0); //Inicializo
	carpeta1 = assoofs_create_dir(sb, root, "carpeta1");
	if (carpeta1) //Si se ha creado con éxito
		assoofs_create_file(sb, carpeta1, "contador2", &contador2);
}

//Creación de un fichero
static struct dentry *assoofs_create_file(struct super_block *sb , struct dentry *dir, const char *name , atomic_t *counter){
	struct dentry *dentry;
	struct inode *inode;
	struct qstr qname;
	
	qname.name = name;
	qname.len = strlen(name);
	qname.hash = full_name_hash(name, qname.len);
	
	dentry = d_alloc(dir, &qname);
	
	if(!dentry){
		printk(KERN_ERR "Ha fallado la creación del fichero");
		return NULL;
	}
	inode = assoofs_make_inode(sb, S_IFREG | 0777); //0644
	if(!inode){
		printk(KERN_ERR "Ha fallado la creación del inodo del fichero");
		return NULL;
	}
	inode->i_fop = &assoofs_file_ops;
	inode->i_private = &texto[posicion];
	posicion += TMPSIZE;
	
	//Añade la esctructura dentry a la cache de directorios
	d_add(dentry, inode);
	return dentry;
}


struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags){
	printk(KERN_INFO "Looking up inode...\n");
	return NULL;
}

static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode){
	//El dentry ya lo tengo
	struct inode *inode;
	
	inode = assoofs_make_inode(dir->i_sb, S_IFDIR | 0777);//0755
	if (!inode){
		printk(KERN_ERR "Ha fallado la creación la entrada al directorio");
		return -EFAULT;
	}
	inode->i_op = &assoofs_inode_ops; 
	inode->i_fop = &simple_dir_operations;

	d_add(dentry, inode);
	return 0;
}

static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl){
	//El dentry ya lo tengo
	struct inode *inode;
	
	inode = assoofs_make_inode(dir->i_sb, S_IFREG | 0777); //0644
	if(!inode){
		printk(KERN_ERR "Ha fallado la creación del inodo del fichero");
		return -EFAULT;
	}
	inode->i_fop = &assoofs_file_ops;
	inode->i_private = &texto[posicion];
	posicion += TMPSIZE;
	
	//Añade la esctructura dentry a la cache de directorios
	d_add(dentry, inode);
	return 0;
}

//Creación de un inodo
static struct inode *assoofs_make_inode(struct super_block *sb, int mode){
	struct inode *ret = new_inode(sb);
	if(ret){
		ret->i_mode = mode;
		ret->i_uid.val = ret->i_gid.val = 0;
		ret->i_blocks = 0;
		ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
	}
	return ret;
}

//Abrir
static int assoofs_open (struct inode *inode, struct file *filp){
	filp->private_data = inode->i_private;
	return 0;
}

//Leer
static ssize_t assoofs_read_file ( struct file *filp , char *buf, size_t count, loff_t *offset){
	char tmp[TMPSIZE];
	int len = TMPSIZE; //Longitud máxima a escribir
	
	int nbytes, i;
	char * buffer;
	if (*offset > len) {
		return 0;
	}

	if(len - *offset < count){
		nbytes = len - *offset;
	}else{
		nbytes = count;
	}
	
	buffer = (char *) filp->private_data;
	
	for(i = 0; i < TMPSIZE; i++){
		tmp[i] = buffer[i];
	}
	
	if (copy_to_user(buf, tmp + *offset, nbytes)) {
		printk(KERN_ERR "Error al copiar al espacio de usuario\n");
		return -EFAULT;
	}

	*offset += nbytes;

	return nbytes;
}

//Escribir
static ssize_t assoofs_write_file( struct file *filp, const char *buf, size_t count, loff_t *offset){
	char tmp[TMPSIZE];
	int len, i;
	len = TMPSIZE;
	char* buffer = (char *) filp->private_data;
	
	if (*offset != 0)
		return -EINVAL;
		
	if (count >= len)
		return -EINVAL;
		
	memset(tmp, 0, len);
	
	if(copy_from_user(tmp, buf, count))
		return -EFAULT;
		
	for(i = 0; i < len; i++){
		buffer[i] = tmp[i];
	}
	
	return count;
}

//Salir
static void __exit assoofs_exit(void){
	kfree(texto);
	unregister_filesystem(&assoofs_type);
}

module_init(assoofs_init);
module_exit(assoofs_exit);
