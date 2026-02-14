import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Field, Input, makeStyles, Spinner, TableCellLayout, TableColumnDefinition, tokens } from '@fluentui/react-components';
import { startTransition, Suspense, useEffect, useState } from 'react';
import { useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205ListUsers, postApiV1B64Be5124B034028A58913931942E205RemoveUser, postApiV1B64Be5124B034028A58913931942E205AddUser } from '../api';
import { Pages, router, state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular, KeyRegular, PeopleRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type UserType = {
  id: string;
  username: string;
  system?: boolean;
};

function filterUsers(item: UserType, search: string) {
  const searchableFields = {
    username: item.username,
  };

  return filterBySearch(search, searchableFields);
}

function navigateToUserRoles(user: UserType) {
  startTransition(() => {
    router.navigate(`/${Pages.UserRoles}/${encodeURIComponent(user.username)}/${encodeURIComponent(user.id)}`);
  });
}

function navigateToUserAccessKeys(user: UserType) {
  startTransition(() => {
    router.navigate(`/${Pages.UserAccessKeys}/${encodeURIComponent(user.username)}/${encodeURIComponent(user.id)}`);
  });
}

const useStyles = makeStyles({
  gridActions: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    flexWrap: "wrap",
  },
});

const Users = () => {

  const classes = useStyles();
  const queryClient = useQueryClient();

  const snap = useSnapshot(state);

  // Delete user state
  const [isDeleting, setIsDeleting] = useState<string | null>(null);
  const [deleteDialogOpen, setDeleteDialogOpen] = useState(false);
  const [userToDelete, setUserToDelete] = useState<UserType | null>(null);
  const [deleteError, setDeleteError] = useState('');

  // Add user state
  const [addDialogOpen, setAddDialogOpen] = useState(false);
  const [newUsername, setNewUsername] = useState('');
  const [newPassword, setNewPassword] = useState('');
  const [addError, setAddError] = useState('');
  const [isAdding, setIsAdding] = useState(false);

  const handleAddUser = async () => {
    setIsAdding(true);
    setAddError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205AddUser({
        requestBody: { ses: snap.session, username: newUsername, password: newPassword }
      });
      await queryClient.invalidateQueries({ queryKey: ["users", snap.session] });
      setAddDialogOpen(false);
      setNewUsername('');
      setNewPassword('');
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setAddError(e.msg || 'Failed to add user');
      } else {
        setAddError('Failed to add user');
      }
    } finally {
      setIsAdding(false);
    }
  };

  const handleDeleteUser = async (user: UserType) => {
    setIsDeleting(user.id);
    setDeleteError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205RemoveUser({
        requestBody: { ses: snap.session, username: user.username }
      });
      await queryClient.invalidateQueries({ queryKey: ["users", snap.session] });
      setDeleteDialogOpen(false);
      setUserToDelete(null);
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setDeleteError(e.msg || 'Failed to delete user');
      } else {
        setDeleteError('Failed to delete user');
      }
    } finally {
      setIsDeleting(null);
    }
  };

  const columns: TableColumnDefinition<UserType>[] = [
    createTableColumn<UserType>({
      columnId: "username",
      renderHeaderCell: () => {
        return "Username";
      },
      compare: (a, b) => {
        return a.username.localeCompare(b.username);
      },
      renderCell: (item) => {
        return <TableCellLayout>{item.username}</TableCellLayout>;
      },
    }),
    createTableColumn<UserType>({
      columnId: "action",
      renderHeaderCell: () => {
        return "Actions";
      },
      renderCell: (item) => {
        return (
          <TableCellLayout>
            <Button
              icon={<PeopleRegular />}
              onClick={(e) => {
                e.stopPropagation();
                navigateToUserRoles(item);
              }}
            >
              Roles
            </Button>
            <Button
              icon={<KeyRegular />}
              onClick={(e) => {
                e.stopPropagation();
                navigateToUserAccessKeys(item);
              }}
            >
              Access Keys
            </Button>
            <Button
              icon={<DeleteRegular />}
              disabled={isDeleting === item.id || item.system}
              onClick={(e) => {
                e.stopPropagation();
                setUserToDelete(item);
                setDeleteError('');
                setDeleteDialogOpen(true);
              }}
            >
              {isDeleting === item.id ? 'Deleting...' : 'Delete'}
            </Button>
          </TableCellLayout>
        );
      }
    }),
  ];

  const usersResult = useSuspenseQuery({
    queryKey: ["users", snap.session],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListUsers({requestBody: {ses: snap.session}}); },
  });

  const data = usersResult.data!.users as UserType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<UserType>(
    sortedData,
    filterUsers,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Users</h3>
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
              <DataGridBody<UserType>>
                {({ item, rowId }) => (
                  <DataGridRow<UserType>
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
            <div className={classes.gridActions}>
              <Button onClick={() => {
                setNewUsername('');
                setNewPassword('');
                setAddError('');
                setAddDialogOpen(true);
              }} icon={<AddRegular />}>
                Add User
              </Button>
            </div>
        </div>
      </TableWrapper>

      {/* Add User Dialog */}
      <Dialog open={addDialogOpen} onOpenChange={(_, data) => {
        setAddDialogOpen(data.open);
        if (!data.open) {
          setAddError('');
        }
      }}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Add User</DialogTitle>
            <DialogContent>
              <Field 
                label="Username"
                validationMessage={addError}
                validationState={addError ? "error" : "none"}
              >
                <Input
                  value={newUsername}
                  onChange={(e) => setNewUsername(e.target.value)}
                  placeholder="Enter username"
                />
              </Field>
              <Field label="Password" style={{ marginTop: '1rem' }}>
                <Input
                  type="password"
                  value={newPassword}
                  onChange={(e) => setNewPassword(e.target.value)}
                  placeholder="Enter password"
                />
              </Field>
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={handleAddUser}
                disabled={isAdding || !newUsername || !newPassword}
              >
                {isAdding ? 'Adding...' : 'Add'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>

      {/* Delete Confirmation Dialog */}
      <Dialog open={deleteDialogOpen} onOpenChange={(_, data) => {
        setDeleteDialogOpen(data.open);
        if (!data.open) {
          setDeleteError('');
        }
      }}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Delete User</DialogTitle>
            <DialogContent>
              <p>Are you sure you want to delete the user "{userToDelete?.username}"?</p>
              <p>This action cannot be undone. All access keys and role assignments for this user will be removed.</p>
              {deleteError && <p style={{ color: 'red' }}>{deleteError}</p>}
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={() => userToDelete && handleDeleteUser(userToDelete)}
                disabled={isDeleting !== null}
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

export default Users;
