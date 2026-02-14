import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Field, Input, makeStyles, Spinner, TableCellLayout, TableColumnDefinition, tokens, MessageBar, MessageBarTitle, MessageBarBody, MessageBarActions } from '@fluentui/react-components';
import { Suspense, useEffect, useState } from 'react';
import { useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { useParams } from 'react-router-dom';
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205ListAccessKeys, postApiV1B64Be5124B034028A58913931942E205AddAccessKey, postApiV1B64Be5124B034028A58913931942E205RemoveAccessKey } from '../api';
import { state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular, CopyRegular, DismissRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type AccessKeyType = {
  id: string;
  accessKeyId: string;
};

type NewAccessKeyType = {
  id: string;
  accessKey: string;
  secretKey: string;
};

function filterAccessKeys(item: AccessKeyType, search: string) {
  const searchableFields = {
    accessKeyId: item.accessKeyId,
  };

  return filterBySearch(search, searchableFields);
}

const useStyles = makeStyles({
  gridActions: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    flexWrap: "wrap",
  },
  formRow: {
    display: "flex",
    gap: tokens.spacingHorizontalM,
    alignItems: "flex-end",
    marginBottom: tokens.spacingVerticalM,
  },
  newKeyContainer: {
    marginBottom: tokens.spacingVerticalL,
    padding: tokens.spacingHorizontalM,
    backgroundColor: tokens.colorNeutralBackground2,
    borderRadius: tokens.borderRadiusMedium,
  },
  newKeyField: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    alignItems: "center",
    marginBottom: tokens.spacingVerticalS,
  },
  newKeyLabel: {
    minWidth: "120px",
    fontWeight: tokens.fontWeightSemibold,
  },
  newKeyValue: {
    fontFamily: "monospace",
    flex: 1,
  },
  warningText: {
    color: tokens.colorPaletteRedForeground1,
    fontWeight: tokens.fontWeightSemibold,
    marginTop: tokens.spacingVerticalS,
  },
});

const UserAccessKeys = () => {

  const classes = useStyles();
  const queryClient = useQueryClient();
  const { username, userId } = useParams<{ username: string; userId: string }>();

  const snap = useSnapshot(state);

  const accessKeysResult = useSuspenseQuery({
    queryKey: ["accessKeys", snap.session, userId],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListAccessKeys({requestBody: {ses: snap.session, userId: userId!}}); },
  });

  const data = accessKeysResult.data!.accessKeys as AccessKeyType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<AccessKeyType>(
    sortedData,
    filterAccessKeys,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  // Add access key state
  const [isAdding, setIsAdding] = useState(false);
  const [addError, setAddError] = useState('');
  const [newAccessKey, setNewAccessKey] = useState<NewAccessKeyType | null>(null);

  // Remove access key state
  const [isRemoving, setIsRemoving] = useState<string | null>(null);
  const [removeDialogOpen, setRemoveDialogOpen] = useState(false);
  const [keyToRemove, setKeyToRemove] = useState<AccessKeyType | null>(null);

  const handleGenerateAccessKey = async () => {
    setIsAdding(true);
    setAddError('');
    try {
      const result = await postApiV1B64Be5124B034028A58913931942E205AddAccessKey({
        requestBody: { ses: snap.session, userId: userId! }
      });
      setNewAccessKey({
        id: result.id as string,
        accessKey: result.accessKey as string,
        secretKey: result.secretKey as string,
      });
      await queryClient.invalidateQueries({ queryKey: ["accessKeys", snap.session, userId] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setAddError(e.msg || 'Failed to generate access key');
      } else {
        setAddError('Failed to generate access key');
      }
    } finally {
      setIsAdding(false);
    }
  };

  const handleRemoveAccessKey = async (accessKey: AccessKeyType) => {
    setIsRemoving(accessKey.id);
    try {
      await postApiV1B64Be5124B034028A58913931942E205RemoveAccessKey({
        requestBody: { ses: snap.session, id: accessKey.id }
      });
      await queryClient.invalidateQueries({ queryKey: ["accessKeys", snap.session, userId] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        console.error('Failed to remove access key:', e.msg);
      }
    } finally {
      setIsRemoving(null);
      setRemoveDialogOpen(false);
      setKeyToRemove(null);
    }
  };

  const copyToClipboard = async (text: string) => {
    try {
      await navigator.clipboard.writeText(text);
    } catch (err) {
      console.error('Failed to copy:', err);
    }
  };

  const columns: TableColumnDefinition<AccessKeyType>[] = [
    createTableColumn<AccessKeyType>({
      columnId: "accessKeyId",
      renderHeaderCell: () => {
        return "Access Key ID";
      },
      compare: (a, b) => {
        return a.accessKeyId.localeCompare(b.accessKeyId);
      },
      renderCell: (item) => {
        return <TableCellLayout style={{ fontFamily: 'monospace' }}>{item.accessKeyId}</TableCellLayout>;
      },
    }),
    createTableColumn<AccessKeyType>({
      columnId: "action",
      renderHeaderCell: () => {
        return "Actions";
      },
      renderCell: (item) => {
        return (
          <TableCellLayout>
            <Button
              icon={<DeleteRegular />}
              disabled={isRemoving === item.id}
              onClick={(e) => {
                e.stopPropagation();
                setKeyToRemove(item);
                setRemoveDialogOpen(true);
              }}
            >
              {isRemoving === item.id ? 'Removing...' : 'Remove'}
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
        <h3>Access Keys for user: {username}</h3>
        
        {/* Generate Access Key Button */}
        <div className={classes.formRow}>
          <Button
            appearance="primary"
            icon={<AddRegular />}
            onClick={handleGenerateAccessKey}
            disabled={isAdding}
          >
            {isAdding ? 'Generating...' : 'Generate New Access Key'}
          </Button>
          {addError && <span style={{ color: tokens.colorPaletteRedForeground1 }}>{addError}</span>}
        </div>

        {/* New Access Key Display */}
        {newAccessKey && (
          <MessageBar intent="warning" layout="multiline">
            <MessageBarBody>
              <MessageBarTitle>New Access Key Created - Save These Credentials!</MessageBarTitle>
              <div className={classes.newKeyContainer}>
                <div className={classes.newKeyField}>
                  <span className={classes.newKeyLabel}>Access Key ID:</span>
                  <Input
                    className={classes.newKeyValue}
                    value={newAccessKey.accessKey}
                    readOnly
                  />
                  <Button
                    icon={<CopyRegular />}
                    onClick={() => copyToClipboard(newAccessKey.accessKey)}
                    title="Copy Access Key"
                  />
                </div>
                <div className={classes.newKeyField}>
                  <span className={classes.newKeyLabel}>Secret Access Key:</span>
                  <Input
                    className={classes.newKeyValue}
                    value={newAccessKey.secretKey}
                    readOnly
                  />
                  <Button
                    icon={<CopyRegular />}
                    onClick={() => copyToClipboard(newAccessKey.secretKey)}
                    title="Copy Secret Key"
                  />
                </div>
                <p className={classes.warningText}>
                  This is the only time you will see the secret key. Please save it securely now.
                </p>
              </div>
            </MessageBarBody>
            <MessageBarActions
              containerAction={
                <Button
                  aria-label="dismiss"
                  appearance="transparent"
                  icon={<DismissRegular />}
                  onClick={() => setNewAccessKey(null)}
                />
              }
            />
          </MessageBar>
        )}

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
              getRowId={(item) => item.id}
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
              <DataGridBody<AccessKeyType>>
                {({ item, rowId }) => (
                  <DataGridRow<AccessKeyType>
                    key={rowId}
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
        </div>
      </TableWrapper>
    </Suspense>

    {/* Remove Access Key Confirmation Dialog */}
    <Dialog open={removeDialogOpen} onOpenChange={(_, data) => setRemoveDialogOpen(data.open)}>
      <DialogSurface>
        <DialogBody>
          <DialogTitle>Remove Access Key</DialogTitle>
          <DialogContent>
            Are you sure you want to remove access key "{keyToRemove?.accessKeyId}"?
          </DialogContent>
          <DialogActions>
            <DialogTrigger disableButtonEnhancement>
              <Button appearance="secondary">Cancel</Button>
            </DialogTrigger>
            <Button
              appearance="primary"
              onClick={() => keyToRemove && handleRemoveAccessKey(keyToRemove)}
              disabled={isRemoving !== null}
            >
              {isRemoving !== null ? 'Removing...' : 'Remove'}
            </Button>
          </DialogActions>
        </DialogBody>
      </DialogSurface>
    </Dialog>
    </>
  );
};

export default UserAccessKeys;
