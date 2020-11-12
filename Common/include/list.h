#include <common.h>
#ifndef LIST_H
#define LIST_H

struct xLIST;
struct xLIST_ITEM
{
    TickType_t xItemValue;              /*< The value being listed.  In most cases this is used to sort the list in descending order. */
    struct xLIST_ITEM * pxNext;         /*< Pointer to the next ListItem_t in the list. */
    struct xLIST_ITEM * pxPrevious;     /*< Pointer to the previous ListItem_t in the list. */
    void * pvOwner;                                         /*< Pointer to the object (normally a TCB) that contains the list item.  There is therefore a two way link between the object containing the list item and the list item itself. */
    struct xLIST * pxContainer;         /*< Pointer to the list in which this list item is placed (if any). */
};
typedef struct xLIST_ITEM ListItem_t;                       /* For some reason lint wants this as two separate definitions. */

struct xMINI_LIST_ITEM
{
    TickType_t xItemValue;
    struct xLIST_ITEM * pxNext;
    struct xLIST_ITEM * pxPrevious;
};
typedef struct xMINI_LIST_ITEM MiniListItem_t;
typedef struct xLIST
{
    volatile UBaseType_t uxNumberOfItems;
    ListItem_t * pxIndex;     
    MiniListItem_t xListEnd;                      
}List_t;

/*
 * Definition of the type of queue used by the scheduler.
 */


#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )    ( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

#define listGET_LIST_ITEM_OWNER( pxListItem )             ( ( pxListItem )->pvOwner )

#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )     ( ( pxListItem )->xItemValue = ( xValue ) )

#define listGET_LIST_ITEM_VALUE( pxListItem )             ( ( pxListItem )->xItemValue )

#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )        ( ( ( pxList )->xListEnd ).pxNext->xItemValue )

#define listGET_HEAD_ENTRY( pxList )                      ( ( ( pxList )->xListEnd ).pxNext )

#define listGET_NEXT( pxListItem )                        ( ( pxListItem )->pxNext )

#define listGET_END_MARKER( pxList )                      ( ( ListItem_t const * ) ( &( ( pxList )->xListEnd ) ) )

#define listLIST_IS_EMPTY( pxList )                       ( ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) ? pdTRUE : pdFALSE )

#define listCURRENT_LIST_LENGTH( pxList )                 ( ( pxList )->uxNumberOfItems )

#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )                                           \
    {                                                                                          \
        List_t * const pxConstList = ( pxList );                                               \
        /* Increment the index to the next item and return the item, ensuring */               \
        /* we don't return the marker used at the end of the list.  */                         \
        ( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;                           \
        if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) ) \
        {                                                                                      \
            ( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;                       \
        }                                                                                      \
        ( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;                                         \
    }


#define listGET_OWNER_OF_HEAD_ENTRY( pxList )            ( ( &( ( pxList )->xListEnd ) )->pxNext->pvOwner )

#define listIS_CONTAINED_WITHIN( pxList, pxListItem )    ( ( ( pxListItem )->pxContainer == ( pxList ) ) ? ( pdTRUE ) : ( pdFALSE ) )

#define listLIST_ITEM_CONTAINER( pxListItem )            ( ( pxListItem )->pxContainer )

#define listLIST_IS_INITIALISED( pxList )                ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )
/*
void vListInitialise(List_t * const pxList);

void vListInitialiseItem(ListItem_t * const pxItem);


void vListInsert(List_t * const pxList,ListItem_t * const pxNewListItem);

void vListInsertEnd(List_t * const pxList,ListItem_t * const pxNewListItem);

UBaseType_t uxListRemove(ListItem_t * const pxItemToRemove);
*/

#endif
