import { Button, Checkbox, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Dropdown, Field, makeStyles, MessageBar, MessageBarBody, MessageBarTitle, Option, Spinner, TableCellLayout, TableColumnDefinition, tokens } from '@fluentui/react-components';
import { FormEvent, Suspense, useEffect, useState } from 'react';
import { useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { useParams } from 'react-router-dom';
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, listBucketPermissions, addBucketPermission, removeBucketPermission, listUsers, setBucketPublic } from '../api';
import { state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type BucketPermissionType = {
  id: string;
  userId: string;
  userName: string;
  permissions: Array<'read' | 'write' | 'delete'>;
};

type UserType = {
  id: string;
  username: string;
};

function filterPermissions(item: BucketPermissionType, search: string) {
  const searchableFields = {
    userName: item.userName,
    permissions: item.permissions.join(', '),
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
  checkboxGroup: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    alignItems: "center",
  },
});

const BucketPermissions = () => {
  const classes = useStyles();
  const queryClient = useQueryClient();
  const { bucketName } = useParams<{ bucketName: string }>();
  const snap = useSnapshot(state);

  const permissionsResult = useSuspenseQuery({
    queryKey: ["bucketPermissions", snap.session, bucketName],
    queryFn: async () => { return await listBucketPermissions({ requestBody: { bucketName: bucketName! } }); },
  });

  const usersResult = useSuspenseQuery({
    queryKey: ["users", snap.session],
    queryFn: async () => { return await listUsers(); },
  });

  const data = permissionsResult.data!.bucketPermissions as BucketPermissionType[];
  const allUsers = usersResult.data!.users as UserType[];
  const fetchedPublicPermissions = permissionsResult.data!.publicPermissions as Array<'read' | 'write' | 'delete'>;

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<BucketPermissionType>(
    sortedData,
    filterPermissions,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  // Public permissions state
  const [publicRead, setPublicRead] = useState(() => fetchedPublicPermissions.includes('read'));
  const [publicWrite, setPublicWrite] = useState(() => fetchedPublicPermissions.includes('write'));
  const [publicDelete, setPublicDelete] = useState(() => fetchedPublicPermissions.includes('delete'));
  const [isUpdatingPublic, setIsUpdatingPublic] = useState(false);
  const [updatePublicError, setUpdatePublicError] = useState('');

  useEffect(() => {
    setPublicRead(fetchedPublicPermissions.includes('read'));
    setPublicWrite(fetchedPublicPermissions.includes('write'));
    setPublicDelete(fetchedPublicPermissions.includes('delete'));
  }, [fetchedPublicPermissions]);

  const handleSetPublicPermissions = async () => {
    const perms: Array<'read' | 'write' | 'delete'> = [];
    if (publicRead) perms.push('read');
    if (publicWrite) perms.push('write');
    if (publicDelete) perms.push('delete');

    setIsUpdatingPublic(true);
    setUpdatePublicError('');
    try {
      await setBucketPublic({
        requestBody: { bucketName: bucketName!, public: perms }
      });
      await queryClient.invalidateQueries({ queryKey: ["bucketPermissions", snap.session, bucketName] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setUpdatePublicError(e.msg || 'Failed to update public permissions');
      } else {
        setUpdatePublicError('Failed to update public permissions');
      }
    } finally {
      setIsUpdatingPublic(false);
    }
  };

  // Add permission state
  const [selectedUserId, setSelectedUserId] = useState<string>('');
  const [permRead, setPermRead] = useState(true);
  const [permWrite, setPermWrite] = useState(false);
  const [permDelete, setPermDelete] = useState(false);
  const [isAdding, setIsAdding] = useState(false);
  const [addError, setAddError] = useState('');

  // Remove permission state
  const [isRemoving, setIsRemoving] = useState<string | null>(null);
  const [removeDialogOpen, setRemoveDialogOpen] = useState(false);
  const [permToRemove, setPermToRemove] = useState<BucketPermissionType | null>(null);

  const handleAddPermission = async (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!selectedUserId) return;

    const perms: Array<'read' | 'write' | 'delete'> = [];
    if (permRead) perms.push('read');
    if (permWrite) perms.push('write');
    if (permDelete) perms.push('delete');

    if (perms.length === 0) {
      setAddError('Select at least one permission');
      return;
    }

    setIsAdding(true);
    setAddError('');
    try {
      await addBucketPermission({
        requestBody: { userId: selectedUserId, bucketName: bucketName!, addBucketPermissions: perms }
      });
      setSelectedUserId('');
      setPermRead(true);
      setPermWrite(false);
      setPermDelete(false);
      await queryClient.invalidateQueries({ queryKey: ["bucketPermissions", snap.session, bucketName] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setAddError(e.msg || 'Failed to add permission');
      } else {
        setAddError('Failed to add permission');
      }
    } finally {
      setIsAdding(false);
    }
  };

  const handleRemovePermission = async (perm: BucketPermissionType) => {
    setIsRemoving(perm.id);
    try {
      await removeBucketPermission({
        requestBody: { id: perm.id }
      });
      await queryClient.invalidateQueries({ queryKey: ["bucketPermissions", snap.session, bucketName] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        console.error('Failed to remove permission:', e.msg);
      }
    } finally {
      setIsRemoving(null);
      setRemoveDialogOpen(false);
      setPermToRemove(null);
    }
  };

  const columns: TableColumnDefinition<BucketPermissionType>[] = [
    createTableColumn<BucketPermissionType>({
      columnId: "userName",
      renderHeaderCell: () => "User",
      compare: (a, b) => a.userName.localeCompare(b.userName),
      renderCell: (item) => <TableCellLayout>{item.userName}</TableCellLayout>,
    }),
    createTableColumn<BucketPermissionType>({
      columnId: "permissions",
      renderHeaderCell: () => "Permissions",
      renderCell: (item) => <TableCellLayout>{item.permissions.join(', ')}</TableCellLayout>,
    }),
    createTableColumn<BucketPermissionType>({
      columnId: "action",
      renderHeaderCell: () => "Actions",
      renderCell: (item) => (
        <TableCellLayout>
          <Button
            icon={<DeleteRegular />}
            disabled={isRemoving === item.id}
            onClick={(e) => {
              e.stopPropagation();
              setPermToRemove(item);
              setRemoveDialogOpen(true);
            }}
          >
            {isRemoving === item.id ? 'Removing...' : 'Remove'}
          </Button>
        </TableCellLayout>
      ),
    }),
  ];

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Simple user permissions for bucket: {bucketName}</h3>

        {fetchedPublicPermissions.length > 0 && (
          <MessageBar intent="warning" style={{ marginBottom: tokens.spacingVerticalM }}>
            <MessageBarBody>
              <MessageBarTitle>Public access enabled</MessageBarTitle>
              This bucket is accessible by everyone ({fetchedPublicPermissions.join(', ')}).
            </MessageBarBody>
          </MessageBar>
        )}

        {/* Public Permissions */}
        <div className={classes.formRow}>
          <Field label="Public permissions" validationMessage={updatePublicError} validationState={updatePublicError ? 'error' : 'none'}>
            <div className={classes.checkboxGroup}>
              <Checkbox label="Read" checked={publicRead} onChange={(_, d) => setPublicRead(!!d.checked)} disabled={isUpdatingPublic} />
              <Checkbox label="Write" checked={publicWrite} onChange={(_, d) => setPublicWrite(!!d.checked)} disabled={isUpdatingPublic} />
              <Checkbox label="Delete" checked={publicDelete} onChange={(_, d) => setPublicDelete(!!d.checked)} disabled={isUpdatingPublic} />
            </div>
          </Field>
          <Button appearance="primary" onClick={handleSetPublicPermissions} disabled={isUpdatingPublic}>
            {isUpdatingPublic ? 'Saving...' : 'Save Public Permissions'}
          </Button>
        </div>

        {/* Add Permission Form */}
        <form onSubmit={handleAddPermission} className={classes.formRow}>
          <Field label="User" validationMessage={addError} validationState={addError ? "error" : "none"}>
            <Dropdown
              placeholder="Select a user"
              value={allUsers.find(u => u.id === selectedUserId)?.username || ''}
              onOptionSelect={(_, data) => setSelectedUserId(data.optionValue || '')}
              disabled={isAdding}
            >
              {allUsers.map((user) => (
                <Option key={user.id} value={user.id}>
                  {user.username}
                </Option>
              ))}
            </Dropdown>
          </Field>
          <div className={classes.checkboxGroup}>
            <Checkbox label="Read" checked={permRead} onChange={(_, data) => setPermRead(!!data.checked)} />
            <Checkbox label="Write" checked={permWrite} onChange={(_, data) => setPermWrite(!!data.checked)} />
            <Checkbox label="Delete" checked={permDelete} onChange={(_, data) => setPermDelete(!!data.checked)} />
          </div>
          <Button type="submit" icon={<AddRegular />} appearance="primary" disabled={isAdding || !selectedUserId}>
            {isAdding ? 'Adding...' : 'Add Permission'}
          </Button>
        </form>

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
              <DataGridBody<BucketPermissionType>>
                {({ item, rowId }) => (
                  <DataGridRow<BucketPermissionType>
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

      {/* Remove Confirmation Dialog */}
      <Dialog open={removeDialogOpen} onOpenChange={(_, data) => setRemoveDialogOpen(data.open)}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Remove Permission</DialogTitle>
            <DialogContent>
              Are you sure you want to remove permissions for user "{permToRemove?.userName}"?
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={() => permToRemove && handleRemovePermission(permToRemove)}
                disabled={isRemoving !== null}
              >
                {isRemoving !== null ? 'Removing...' : 'Remove'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>
    </Suspense>
    </>
  );
};

export default BucketPermissions;
