 /*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * Ce logiciel est un serveur implementant le protocole NFS.
 *
 * Ce logiciel est r�gi par la licence CeCILL soumise au droit fran�ais et
 * respectant les principes de diffusion des logiciels libres. Vous pouvez
 * utiliser, modifier et/ou redistribuer ce programme sous les conditions
 * de la licence CeCILL telle que diffus�e par le CEA, le CNRS et l'INRIA
 * sur le site "http://www.cecill.info".
 *
 * En contrepartie de l'accessibilit� au code source et des droits de copie,
 * de modification et de redistribution accord�s par cette licence, il n'est
 * offert aux utilisateurs qu'une garantie limit�e.  Pour les m�mes raisons,
 * seule une responsabilit� restreinte p�se sur l'auteur du programme,  le
 * titulaire des droits patrimoniaux et les conc�dants successifs.
 *
 * A cet �gard  l'attention de l'utilisateur est attir�e sur les risques
 * associ�s au chargement,  � l'utilisation,  � la modification et/ou au
 * d�veloppement et � la reproduction du logiciel par l'utilisateur �tant
 * donn� sa sp�cificit� de logiciel libre, qui peut le rendre complexe �
 * manipuler et qui le r�serve donc � des d�veloppeurs et des professionnels
 * avertis poss�dant  des  connaissances  informatiques approfondies.  Les
 * utilisateurs sont donc invit�s � charger  et  tester  l'ad�quation  du
 * logiciel � leurs besoins dans des conditions permettant d'assurer la
 * s�curit� de leurs syst�mes et ou de leurs donn�es et, plus g�n�ralement,
 * � l'utiliser et l'exploiter dans les m�mes conditions de s�curit�.
 *
 * Le fait que vous puissiez acc�der � cet en-t�te signifie que vous avez
 * pris connaissance de la licence CeCILL, et que vous en avez accept� les
 * termes.
 *
 * ---------------------
 *
 * Copyright CEA/DAM/DIF (2005)
 *  Contributor: Philippe DENIEL  philippe.deniel@cea.fr
 *               Thomas LEIBOVICI thomas.leibovici@cea.fr
 *
 *
 * This software is a server that implements the NFS protocol.
 * 
 *
 * This software is governed by the CeCILL  license under French law and
 * abiding by the rules of distribution of free software.  You can  use,
 * modify and/ or redistribute the software under the terms of the CeCILL
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and,  more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license and that you accept its terms.
 * ---------------------------------------
 */

/**
 * \file    nfs4_op_create.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:02:50 $
 * \version $Revision: 1.18 $
 * \brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * nfs4_op_create.c : Routines used for managing the NFS4 COMPOUND functions.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>  /* for having FNDELAY */
#include "HashData.h"
#include "HashTable.h"
#ifdef _USE_GSSRPC
#include <gssrpc/types.h>
#include <gssrpc/rpc.h>
#include <gssrpc/auth.h>
#include <gssrpc/pmap_clnt.h>
#else
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpc/pmap_clnt.h>
#endif

#include "log_functions.h"
#include "stuff_alloc.h"
#include "nfs23.h"
#include "nfs4.h"
#include "mount.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "cache_content.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_tools.h"
#include "nfs_file_handle.h"


/**
 * nfs4_op_create: NFS4_OP_CREATE, creates a non-regular entry.
 * 
 * NFS4_OP_CREATE, creates a non-regular entry.
 *
 * @param op    [IN]    pointer to nfs4_op arguments
 * @param data  [INOUT] Pointer to the compound request's data
 * @param resp  [IN]    Pointer to nfs4_op results
 *
 * @return NFS4_OK if successfull, other values show an error.  
 * 
 */

#define arg_CREATE4 op->nfs_argop4_u.opcreate
#define res_CREATE4 resp->nfs_resop4_u.opcreate

int nfs4_op_create(  struct nfs_argop4 * op ,   
                     compound_data_t   * data,
                     struct nfs_resop4 * resp)
{
  cache_entry_t      * pentry_parent = NULL ;
  cache_entry_t      * pentry_new = NULL ;

  fsal_attrib_list_t   attr_parent ;
  fsal_attrib_list_t   attr_new ;
  fsal_attrib_list_t   sattr ;

  fsal_handle_t      * pnewfsal_handle = NULL ;
  
  nfs_fh4              newfh4 ;
  cache_inode_status_t cache_status ;
  int                  convrc = 0 ;
  
  fsal_accessmode_t mode = 0000 ;
  fsal_name_t       name ;
  
  cache_inode_create_arg_t create_arg;

  char            __attribute__(( __unused__ )) funcname[] = "nfs4_op_create" ;
  unsigned int    i = 0 ;  
  
  resp->resop = NFS4_OP_CREATE ;
  res_CREATE4.status =  NFS4_OK  ;
  
  /* If the filehandle is Empty */
  if( nfs4_Is_Fh_Empty( &(data->currentFH  ) ) )
    {
      res_CREATE4.status = NFS4ERR_NOFILEHANDLE ;
      return res_CREATE4.status ;
    }
  
  /* If the filehandle is invalid */
  if( nfs4_Is_Fh_Invalid( &(data->currentFH ) ) )
    {
      res_CREATE4.status = NFS4ERR_BADHANDLE ;
      return res_CREATE4.status ;
    }
  
  /* Tests if the Filehandle is expired (for volatile filehandle) */
  if( nfs4_Is_Fh_Expired( &(data->currentFH) ) )
    {
      res_CREATE4.status = NFS4ERR_FHEXPIRED ;
      return res_CREATE4.status ;
    }
  
  /* Pseudo Fs is explictely a Read-Only File system */
  if( nfs4_Is_Fh_Pseudo(  &(data->currentFH) ) )
    {
      res_CREATE4.status = NFS4ERR_ROFS ;
      return res_CREATE4.status ;
    }

  /* Ask only for supported attributes */
  if( !nfs4_Fattr_Supported( &arg_CREATE4.createattrs ) )
    {
       res_CREATE4.status = NFS4ERR_ATTRNOTSUPP ;
       return res_CREATE4.status ;
    }

  /* Do not use READ attr, use WRITE attr */
  if( !nfs4_Fattr_Check_Access( &arg_CREATE4.createattrs, FATTR4_ATTR_WRITE ) )
    {
      res_CREATE4.status = NFS4ERR_INVAL ;
      return res_CREATE4.status ;
    }

  /* Check for name to long */
  if( arg_CREATE4.objname.utf8string_len > FSAL_MAX_NAME_LEN )
    {
      res_CREATE4.status = NFS4ERR_NAMETOOLONG ;
      return res_CREATE4.status ;
    }
  
  /* 
   * This operation is used to create a non-regular file, 
   * this means: - a symbolic link
   *             - a block device file
   *             - a character device file
   *             - a socket file
   *             - a fifo
   *             - a directory 
   *
   * You can't use this operation to create a regular file, you have to use NFS4_OP_OPEN for this
   */
  
  /* Convert the UFT8 objname to a regular string */
  if( arg_CREATE4.objname.utf8string_len == 0 )
    {
      res_CREATE4.status = NFS4ERR_INVAL ;
      return res_CREATE4.status ;
    }
  
  if( utf82str( name.name, &arg_CREATE4.objname ) == -1 )
    {
      res_CREATE4.status = NFS4ERR_INVAL ;
      return res_CREATE4.status ;
    }
  name.len = strlen( name.name ) ;
 
   /* Sanuty check: never create a directory named '.' or '..' */
   if(  arg_CREATE4.objtype.type == NF4DIR  )
     {
  	if( !FSAL_namecmp( &name, &FSAL_DOT ) || !FSAL_namecmp( &name, &FSAL_DOT_DOT )  )
    	  {
      	     res_CREATE4.status = NFS4ERR_BADNAME ;
      	     return res_CREATE4.status ;
    	  }

     }

  /* Filename should contain not slash */
  for( i = 0 ; i < name.len ; i ++ )
    {
	if( name.name[i] == '/' ) 
	  {
      	     res_CREATE4.status = NFS4ERR_BADCHAR ;
      	     return res_CREATE4.status ;
	  }
    } 
  /* Convert current FH into a cached entry, the current_pentry (assocated with the current FH will be used for this */
  pentry_parent = data->current_entry ;

  /* The currentFH must point to a directory (objects are always created within a directory) */
  if( data->current_filetype != DIR_BEGINNING && data->current_filetype != DIR_CONTINUE )
    {
      res_CREATE4.status = NFS4ERR_NOTDIR ;
      return res_CREATE4.status ;
    }
  
  /* get attributes of parent directory, for 'change4' info replyed */
  if( ( cache_status = cache_inode_getattr( pentry_parent, 
                                            &attr_parent,
                                            data->ht,
                                            data->pclient, 
                                            data->pcontext, 
                                            &cache_status) ) != CACHE_INODE_SUCCESS )
    {
      res_CREATE4.status = nfs4_Errno( cache_status ) ;
      return res_CREATE4.status ;
    }
  /* Change info for client cache coherency, pentry internal_md is used for that */
  memset( &(res_CREATE4.CREATE4res_u.resok4.cinfo.before), 0, sizeof( changeid4 ) ) ;
  res_CREATE4.CREATE4res_u.resok4.cinfo.before = (changeid4)pentry_parent->internal_md.mod_time;

  /* Convert the incoming fattr4 to a vattr structure, if such arguments are supplied */
  if( arg_CREATE4.createattrs.attrmask.bitmap4_len != 0 )
    {
      /* Arguments were supplied, extract them */
      convrc = nfs4_Fattr_To_FSAL_attr( &sattr, &(arg_CREATE4.createattrs) ) ; 

      if( convrc == 0 )
        {
          res_CREATE4.status = NFS4ERR_ATTRNOTSUPP  ;
          return res_CREATE4.status ;
        }

      if( convrc == -1 )
        {
          res_CREATE4.status = NFS4ERR_BADXDR ;
          return res_CREATE4.status ;
        }
    }
  
  /* Create either a symbolic link or a directory */
  switch( arg_CREATE4.objtype.type ) 
    {
    case NF4LNK :
      /* Convert the name to link from into a regular string */
      if( arg_CREATE4.objtype.createtype4_u.linkdata.utf8string_len == 0 )
        {
          res_CREATE4.status = NFS4ERR_INVAL ;
          return res_CREATE4.status ;
        }
      else
        {
          if( utf82str( create_arg.link_content.path, &arg_CREATE4.objtype.createtype4_u.linkdata ) == -1 )
            {
              res_CREATE4.status = NFS4ERR_INVAL ;
              return res_CREATE4.status ;
            }
          create_arg.link_content.len = strlen( create_arg.link_content.path ) ;
        }
      
      /* do the symlink operation */
      if( ( pentry_new = cache_inode_create( pentry_parent,
                                             &name, 
                                             SYMBOLIC_LINK, 
                                             mode, 
                                             &create_arg, 
                                             &attr_new,
                                             data->ht, 
                                             data->pclient, 
                                             data->pcontext, 
                                             &cache_status ) ) == NULL )
         {
           res_CREATE4.status = nfs4_Errno( cache_status ) ;
           return res_CREATE4.status ;
         }      

     /* If entry exists pentry_new is not null but cache_status was set */
     if( cache_status == CACHE_INODE_ENTRY_EXISTS )
       {
         res_CREATE4.status = NFS4ERR_EXIST ;
         return res_CREATE4.status ;
       }


            
      break ;
    case NF4DIR:
      /* Create a new directory */
      /* do the symlink operation */
      if( ( pentry_new = cache_inode_create( pentry_parent,
                                             &name, 
                                             DIR_BEGINNING,
                                             mode, 
                                             &create_arg, 
                                             &attr_new,
                                             data->ht, 
                                             data->pclient, 
                                             data->pcontext, 
                                             &cache_status ) ) == NULL )
         {
           res_CREATE4.status = nfs4_Errno( cache_status ) ;
           return res_CREATE4.status ;
         }      
         
      /* If entry exists pentry_new is not null but cache_status was set */
      if( cache_status == CACHE_INODE_ENTRY_EXISTS )
       {
          res_CREATE4.status = NFS4ERR_EXIST ;
          return res_CREATE4.status ;
       }    
      break ;
      
    case NF4SOCK:
      
      /* Create a new socket file */
      if( ( pentry_new = cache_inode_create( pentry_parent,
                                             &name, 
                                             SOCKET_FILE,
                                             mode,
                                             NULL,
                                             &attr_new,
                                             data->ht,
                                             data->pclient,
                                             data->pcontext,
                                             &cache_status ) ) == NULL )
      {
        res_CREATE4.status = nfs4_Errno( cache_status ) ;
        return res_CREATE4.status ;
      }      

      /* If entry exists pentry_new is not null but cache_status was set */
      if( cache_status == CACHE_INODE_ENTRY_EXISTS )
      {
        res_CREATE4.status = NFS4ERR_EXIST ;
        return res_CREATE4.status ;
      }   
     break ;

    case NF4FIFO:
      
      /* Create a new socket file */
      if( ( pentry_new = cache_inode_create( pentry_parent,
                                             &name, 
                                             FIFO_FILE,
                                             mode,
                                             NULL,
                                             &attr_new,
                                             data->ht,
                                             data->pclient,
                                             data->pcontext,
                                             &cache_status ) ) == NULL )
      {
        res_CREATE4.status = nfs4_Errno( cache_status ) ;
        return res_CREATE4.status ;
      }      

      /* If entry exists pentry_new is not null but cache_status was set */
      if( cache_status == CACHE_INODE_ENTRY_EXISTS )
      {
        res_CREATE4.status = NFS4ERR_EXIST ;
        return res_CREATE4.status ;
      }   
     break ;

    case NF4CHR:
      
      create_arg.dev_spec.major = arg_CREATE4.objtype.createtype4_u.devdata.specdata1;
      create_arg.dev_spec.minor = arg_CREATE4.objtype.createtype4_u.devdata.specdata2;
      
      /* Create a new socket file */
      if( ( pentry_new = cache_inode_create( pentry_parent,
                                             &name, 
                                             CHARACTER_FILE,
                                             mode,
                                             &create_arg,
                                             &attr_new,
                                             data->ht,
                                             data->pclient,
                                             data->pcontext,
                                             &cache_status ) ) == NULL )
      {
        res_CREATE4.status = nfs4_Errno( cache_status ) ;
        return res_CREATE4.status ;
      }      

      /* If entry exists pentry_new is not null but cache_status was set */
      if( cache_status == CACHE_INODE_ENTRY_EXISTS )
      {
        res_CREATE4.status = NFS4ERR_EXIST ;
        return res_CREATE4.status ;
      }   
     break ;
     
    case NF4BLK:
      
      create_arg.dev_spec.major = arg_CREATE4.objtype.createtype4_u.devdata.specdata1;
      create_arg.dev_spec.minor = arg_CREATE4.objtype.createtype4_u.devdata.specdata2;
      
      /* Create a new socket file */
      if( ( pentry_new = cache_inode_create( pentry_parent,
                                             &name, 
                                             BLOCK_FILE,
                                             mode,
                                             &create_arg,
                                             &attr_new,
                                             data->ht,
                                             data->pclient,
                                             data->pcontext,
                                             &cache_status ) ) == NULL )
      {
        res_CREATE4.status = nfs4_Errno( cache_status ) ;
        return res_CREATE4.status ;
      }      

      /* If entry exists pentry_new is not null but cache_status was set */
      if( cache_status == CACHE_INODE_ENTRY_EXISTS )
      {
        res_CREATE4.status = NFS4ERR_EXIST ;
        return res_CREATE4.status ;
      }   
     break ;
          
    default:
      /* Should never happen, but return NFS4ERR_BADTYPE in this case */
      res_CREATE4.status = NFS4ERR_BADTYPE ;
      return  res_CREATE4.status ;
      break ;
    } /* switch( arg_CREATE4.objtype.type ) */
  
  
  /* Now produce the filehandle to this file */
  if( ( pnewfsal_handle = cache_inode_get_fsal_handle( pentry_new, &cache_status ) ) == NULL )
    {
      res_CREATE4.status = nfs4_Errno( cache_status ) ;
      return  res_CREATE4.status ;
    }
 
  /* Allocation of a new file handle */
  if( nfs4_AllocateFH( &newfh4 ) != NFS4_OK )
    {
      res_CREATE4.status = NFS4ERR_SERVERFAULT ;
      return res_CREATE4.status ;
    }

  /* Building the new file handle */
  if( !nfs4_FSALToFhandle( &newfh4, pnewfsal_handle, data ) )
    {
      res_CREATE4.status = NFS4ERR_SERVERFAULT ;
      return  res_CREATE4.status ;
    }
  
  /* This new fh replaces the current FH */
  data->currentFH.nfs_fh4_len = newfh4.nfs_fh4_len ;
  memcpy( data->currentFH.nfs_fh4_val, newfh4.nfs_fh4_val, newfh4.nfs_fh4_len ) ;
  
  /* No do not need newfh any more */
  Mem_Free( (char *)newfh4.nfs_fh4_val ) ;
  
  /* Set the mode if requested */
  /* Use the same fattr mask for reply, if one attribute was not settable, NFS4ERR_ATTRNOTSUPP was replyied */
  res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_len = arg_CREATE4.createattrs.attrmask.bitmap4_len ;
  
  if( arg_CREATE4.createattrs.attrmask.bitmap4_len != 0 )
    {
      if( ( cache_status = cache_inode_setattr( pentry_new,
                                                &sattr,
                                                data->ht, 
                                                data->pclient, 
                                                data->pcontext, 
                                                &cache_status ) ) != CACHE_INODE_SUCCESS )
        
        {
          res_CREATE4.status = nfs4_Errno( cache_status ) ;
          return res_CREATE4.status ;
        }
      
      /* Allocate a new bitmap */
      res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_val = 
        (unsigned int *)Mem_Alloc( res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_len * sizeof( u_int ) ) ;

      if( res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_val == NULL )
        {
          res_CREATE4.status = NFS4ERR_SERVERFAULT ;
          return res_CREATE4.status ;
        }
      memset( res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_val, 0, res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_len ) ;

      memcpy( res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_val, 
              arg_CREATE4.createattrs.attrmask.bitmap4_val, 
              res_CREATE4.CREATE4res_u.resok4.attrset.bitmap4_len * sizeof( u_int ) ) ;
    }
  
  /* Get the change info on parent directory after the operation was successfull */
  if( ( cache_status = cache_inode_getattr( pentry_parent, 
                                            &attr_parent, 
                                            data->ht, 
                                            data->pclient, 
                                            data->pcontext, 
                                            &cache_status ) ) != CACHE_INODE_SUCCESS )
    {
      res_CREATE4.status = nfs4_Errno( cache_status ) ;
      return res_CREATE4.status ;
    } 
  memset( &(res_CREATE4.CREATE4res_u.resok4.cinfo.after), 0, sizeof( changeid4 ) );
  res_CREATE4.CREATE4res_u.resok4.cinfo.after = (changeid4)pentry_parent->internal_md.mod_time ;
  
  /* Operation is supposed to be atomic .... */
  res_CREATE4.CREATE4res_u.resok4.cinfo.atomic = TRUE ;
  
#ifdef _DEBUG_NFS_V4
  printf( "           CREATE CINFO before = %llu  after = %llu  atomic = %d\n", 
          res_CREATE4.CREATE4res_u.resok4.cinfo.before, 
          res_CREATE4.CREATE4res_u.resok4.cinfo.after, 
          res_CREATE4.CREATE4res_u.resok4.cinfo.atomic ) ;
#endif

  /* @todo : BUGAZOMEU: fair ele free dans cette fonction */
  
  /* Keep the vnode entry for the file in the compound data */
  data->current_entry = pentry_new ;
  data->current_filetype = pentry_new->internal_md.type ;
  
  /* If you reach this point, then no error occured */
  res_CREATE4.status = NFS4_OK ;
  
  return res_CREATE4.status;
} /* nfs4_op_create */


/**
 * nfs4_op_create_Free: frees what was allocared to handle nfs4_op_create.
 * 
 * Frees what was allocared to handle nfs4_op_create.
 *
 * @param resp  [INOUT]    Pointer to nfs4_op results
 *
 * @return nothing (void function )
 * 
 */
void nfs4_op_create_Free( CREATE4res * resp )
{
  if( resp->status == NFS4_OK )
    Mem_Free( resp->CREATE4res_u.resok4.attrset.bitmap4_val ) ;

  return ;
} /* nfs4_op_create_Free */