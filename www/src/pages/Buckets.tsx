import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Field, Input, makeStyles, Spinner, TableCellLayout, TableColumnDefinition, tokens } from '@fluentui/react-components';
import { FormEvent, startTransition, Suspense, useEffect, useState } from 'react';
import { useQuery, useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205List, PostApiV1B64Be5124B034028A58913931942E205ListResponse, postApiV1B64Be5124B034028A58913931942E205DeleteBucket } from '../api';
import { Pages, router, state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular, FolderOpenRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type BucketType = {
  name: string;
  size?: number;
};

function filterBuckets(item: BucketType, search: string) {

  // Search in fields as displayed in the table
  const searchableFields = {
    name: item.name,
  };

  return filterBySearch(search, searchableFields);
}

const useStyles = makeStyles({
  gridActions: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    flexWrap: "wrap",
  },
});

function browseToBucket(item: BucketType)
{
  localStorage.setItem("bucket", item.name);
  window.open("explorer-87652e131c4b45bd851a74de6518889e/explorer.html", "_blank");
}

const Buckets = () => {

  const classes = useStyles();
  const queryClient = useQueryClient();

  const snap = useSnapshot(state);

  const bucketsResult = useSuspenseQuery({
    queryKey: ["buckets", snap.session],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205List({requestBody: {ses: snap.session, path: "/"}}); },
  });

  const data = bucketsResult.data!.objects as BucketType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<BucketType>(
    sortedData,
    filterBuckets,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  // Delete bucket state
  const [isDeleting, setIsDeleting] = useState<string | null>(null);
  const [deleteDialogOpen, setDeleteDialogOpen] = useState(false);
  const [bucketToDelete, setBucketToDelete] = useState<BucketType | null>(null);
  const [deleteConfirmText, setDeleteConfirmText] = useState('');
  const [deleteError, setDeleteError] = useState('');

  const handleDeleteBucket = async (bucket: BucketType) => {
    setIsDeleting(bucket.name);
    setDeleteError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205DeleteBucket({
        requestBody: { ses: snap.session, bucketName: bucket.name }
      });
      await queryClient.invalidateQueries({ queryKey: ["buckets", snap.session] });
      setDeleteDialogOpen(false);
      setBucketToDelete(null);
      setDeleteConfirmText('');
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setDeleteError(e.msg || 'Failed to delete bucket');
      } else {
        setDeleteError('Failed to delete bucket');
      }
    } finally {
      setIsDeleting(null);
    }
  };

  const columns: TableColumnDefinition<BucketType>[] = [
    createTableColumn<BucketType>({
      columnId: "name",
      renderHeaderCell: () => {
        return "Bucket name";
      },
      compare: (a, b) => {
        return a.name.localeCompare(b.name);
      },
      renderCell: (item) => {
        return <TableCellLayout>{item.name}</TableCellLayout>;
      },
    }),
    createTableColumn<BucketType>({
      columnId: "action",
      renderHeaderCell: () => {
        return "Actions";
      },
      renderCell: (item) => {
        return (
          <TableCellLayout>
            <Button
              icon={<FolderOpenRegular />}
              onClick={(e) => {
                e.stopPropagation();
                browseToBucket(item);
              }}
            >
              Browse
            </Button>
            <Button
              icon={<DeleteRegular />}
              disabled={isDeleting === item.name}
              onClick={(e) => {
                e.stopPropagation();
                setBucketToDelete(item);
                setDeleteConfirmText('');
                setDeleteError('');
                setDeleteDialogOpen(true);
              }}
            >
              {isDeleting === item.name ? 'Deleting...' : 'Delete'}
            </Button>
          </TableCellLayout>
        );
      }
    }),
  ];

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Buckets</h3>
        <div className="cluster">
          <SearchBox onSearch={setSearch} />
          <PaginationItemsPerPageSelector
            itemsPerPage={itemsPerPage}
            setItemsPerPage={setItemsPerPage}
          />
          <DataGrid
              key={`${sortState?.sortColumn}-${sortState?.sortDirection}`}
              sortable
              onSortChange={onSortChange}
              items={pageData[page]}
              getRowId={(item) => item.name}
              columns={columns}
            >
              <DataGridHeader>
                <DataGridRow>
                  {({ renderHeaderCell, columnId, compare }) => (
                    <DataGridHeaderCell
                      sortDirection={
                        sortState?.sortColumn === columnId
                          ? sortState.sortDirection
                          : undefined
                      }
                      button={{
                        onClick: () => {
                          const newSortState: typeof sortState = {
                            sortColumn: columnId,
                            sortDirection:
                              sortState?.sortDirection === "descending" ||
                              sortState?.sortColumn !== columnId
                                ? "ascending"
                                : "descending",
                          };

                          if (newSortState.sortDirection === "ascending") {
                            const newSortedData = sortedData.sort(compare);
                            setSortedData(newSortedData);
                          }

                          if (newSortState.sortDirection === "descending") {
                            const newSortedData = sortedData.sort((a, b) =>
                              // Reverse compare function params for descending order
                              compare(b, a),
                            );
                            setSortedData(newSortedData);
                          }

                          setSortState(newSortState);
                        },
                      }}
                    >
                      {renderHeaderCell()}
                    </DataGridHeaderCell>
                  )}
                </DataGridRow>
              </DataGridHeader>
              <DataGridBody<BucketType>>
                {({ item }) => (
                  <DataGridRow<BucketType>
                    key={item.name}
                  >
                    {({ renderCell }) => (
                      <DataGridCell>{renderCell(item)}</DataGridCell>
                    )}
                  </DataGridRow>
                )}
              </DataGridBody>
            </DataGrid>
            <Pagination
              pageCount={pageData.length}
              page={page}
              itemsPerPage={itemsPerPage}
              totalItemCount={filteredItems.length}
              setPage={setPage}
            />
            <div className={classes.gridActions}>
              <Button onClick={() => {
                startTransition(() => {
                  state.pageAfterLogin = Pages.AddBucket;
                  router.navigate(`/${state.pageAfterLogin}`);
                });
              }} icon={<AddRegular />}>
                Add Bucket
              </Button>
            </div>
        </div>
      </TableWrapper>

      {/* Delete Confirmation Dialog */}
      <Dialog open={deleteDialogOpen} onOpenChange={(_, data) => {
        setDeleteDialogOpen(data.open);
        if (!data.open) {
          setDeleteConfirmText('');
          setDeleteError('');
        }
      }}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Delete Bucket</DialogTitle>
            <DialogContent>
              <p>Are you sure you want to delete the bucket "{bucketToDelete?.name}"?</p>
              <p>This action cannot be undone. All objects in the bucket will be permanently deleted.</p>
              <Field 
                label={`Type "${bucketToDelete?.name}" to confirm`}
                validationMessage={deleteError}
                validationState={deleteError ? "error" : "none"}
              >
                <Input
                  value={deleteConfirmText}
                  onChange={(e) => setDeleteConfirmText(e.target.value)}
                  placeholder="Enter bucket name"
                />
              </Field>
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={() => bucketToDelete && handleDeleteBucket(bucketToDelete)}
                disabled={isDeleting !== null || deleteConfirmText !== bucketToDelete?.name}
              >
                {isDeleting !== null ? 'Deleting...' : 'Delete'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>
    </Suspense>
    </>
  );
};

export default Buckets;