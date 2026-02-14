import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Field, Input, makeStyles, Spinner, TableCellLayout, TableColumnDefinition, Textarea, tokens } from '@fluentui/react-components';
import { FormEvent, Suspense, useEffect, useState } from 'react';
import { useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205ListPolicies, postApiV1B64Be5124B034028A58913931942E205AddPolicy, postApiV1B64Be5124B034028A58913931942E205RemovePolicy, postApiV1B64Be5124B034028A58913931942E205ChangePolicy } from '../api';
import { state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular, DocumentRegular, EditRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type PolicyType = {
  id: string;
  name: string;
  document: string;
  system?: boolean;
};

function filterPolicies(item: PolicyType, search: string) {
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
  formContainer: {
    display: "flex",
    flexDirection: "column",
    gap: tokens.spacingVerticalM,
    marginBottom: tokens.spacingVerticalL,
    maxWidth: "600px",
  },
  formRow: {
    display: "flex",
    gap: tokens.spacingHorizontalM,
    alignItems: "flex-start",
  },
  textArea: {
    minHeight: "150px",
  },
  documentPreview: {
    maxHeight: "200px",
    overflow: "auto",
    backgroundColor: tokens.colorNeutralBackground3,
    padding: tokens.spacingHorizontalM,
    borderRadius: tokens.borderRadiusMedium,
    fontFamily: "monospace",
    fontSize: tokens.fontSizeBase200,
    whiteSpace: "pre-wrap",
    wordBreak: "break-all",
  },
});

const Policies = () => {

  const classes = useStyles();
  const queryClient = useQueryClient();

  const snap = useSnapshot(state);

  const policiesResult = useSuspenseQuery({
    queryKey: ["policies", snap.session],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListPolicies({requestBody: {ses: snap.session}}); },
  });

  const data = policiesResult.data!.policies as PolicyType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<PolicyType>(
    sortedData,
    filterPolicies,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  // Add policy state
  const [newPolicyName, setNewPolicyName] = useState('');
  const [newPolicyDocument, setNewPolicyDocument] = useState('');
  const [isAdding, setIsAdding] = useState(false);
  const [addError, setAddError] = useState('');
  const [addDocError, setAddDocError] = useState('');

  // Delete policy state
  const [isDeleting, setIsDeleting] = useState<string | null>(null);
  const [deleteDialogOpen, setDeleteDialogOpen] = useState(false);
  const [policyToDelete, setPolicyToDelete] = useState<PolicyType | null>(null);

  // View policy document state
  const [viewDialogOpen, setViewDialogOpen] = useState(false);
  const [policyToView, setPolicyToView] = useState<PolicyType | null>(null);

  // Edit policy state
  const [editDialogOpen, setEditDialogOpen] = useState(false);
  const [policyToEdit, setPolicyToEdit] = useState<PolicyType | null>(null);
  const [editDocument, setEditDocument] = useState('');
  const [isEditing, setIsEditing] = useState(false);
  const [editError, setEditError] = useState('');

  const handleAddPolicy = async (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!newPolicyName.trim()) return;
    if (!newPolicyDocument.trim()) {
      setAddDocError('Policy document is required');
      return;
    }
    
    setIsAdding(true);
    setAddError('');
    setAddDocError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205AddPolicy({
        requestBody: { 
          ses: snap.session, 
          policyName: newPolicyName.trim(),
          policyDocument: newPolicyDocument.trim()
        }
      });
      setNewPolicyName('');
      setNewPolicyDocument('');
      await queryClient.invalidateQueries({ queryKey: ["policies"] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setAddError(e.msg || 'Failed to add policy');
      } else {
        setAddError('Failed to add policy');
      }
    } finally {
      setIsAdding(false);
    }
  };

  const handleDeletePolicy = async (policy: PolicyType) => {
    setIsDeleting(policy.id);
    try {
      await postApiV1B64Be5124B034028A58913931942E205RemovePolicy({
        requestBody: { ses: snap.session, id: policy.id }
      });
      await queryClient.invalidateQueries({ queryKey: ["policies"] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        console.error('Failed to delete policy:', e.msg);
      }
    } finally {
      setIsDeleting(null);
      setDeleteDialogOpen(false);
      setPolicyToDelete(null);
    }
  };

  const handleEditPolicy = async () => {
    if (!policyToEdit || !editDocument.trim()) return;
    
    setIsEditing(true);
    setEditError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205ChangePolicy({
        requestBody: { 
          ses: snap.session, 
          id: policyToEdit.id,
          document: editDocument.trim()
        }
      });
      await queryClient.invalidateQueries({ queryKey: ["policies"] });
      setEditDialogOpen(false);
      setPolicyToEdit(null);
      setEditDocument('');
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setEditError(e.msg || 'Failed to update policy');
      } else {
        setEditError('Failed to update policy');
      }
    } finally {
      setIsEditing(false);
    }
  };

  const columns: TableColumnDefinition<PolicyType>[] = [
    createTableColumn<PolicyType>({
      columnId: "name",
      renderHeaderCell: () => {
        return "Policy Name";
      },
      compare: (a, b) => {
        return a.name.localeCompare(b.name);
      },
      renderCell: (item) => {
        return <TableCellLayout>{item.name}</TableCellLayout>;
      },
    }),
    createTableColumn<PolicyType>({
      columnId: "action",
      renderHeaderCell: () => {
        return "Actions";
      },
      renderCell: (item) => {
        return (
          <TableCellLayout>
            <Button
              icon={<DocumentRegular />}
              onClick={(e) => {
                e.stopPropagation();
                setPolicyToView(item);
                setViewDialogOpen(true);
              }}
              style={{ marginRight: tokens.spacingHorizontalS }}
            >
              View
            </Button>
            <Button
              icon={<EditRegular />}
              disabled={item.system}
              onClick={(e) => {
                e.stopPropagation();
                setPolicyToEdit(item);
                setEditDocument(item.document);
                setEditError('');
                setEditDialogOpen(true);
              }}
              style={{ marginRight: tokens.spacingHorizontalS }}
            >
              Edit
            </Button>
            <Button
              icon={<DeleteRegular />}
              disabled={isDeleting === item.id || item.system}
              onClick={(e) => {
                e.stopPropagation();
                setPolicyToDelete(item);
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

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Policies</h3>
        
        {/* Add Policy Form */}
        <form onSubmit={handleAddPolicy} className={classes.formContainer}>
          <Field label="Policy Name" validationMessage={addError} validationState={addError ? "error" : "none"} required>
            <Input
              value={newPolicyName}
              onChange={(e) => setNewPolicyName(e.target.value)}
              placeholder="Enter policy name"
              disabled={isAdding}
            />
          </Field>
          <Field label="Policy Document" validationMessage={addDocError} validationState={addDocError ? "error" : "none"} required>
            <Textarea
              value={newPolicyDocument}
              onChange={(e) => setNewPolicyDocument(e.target.value)}
              placeholder='{"Version": "2012-10-17", "Statement": [...]}'
              disabled={isAdding}
              className={classes.textArea}
              resize="vertical"
            />
          </Field>
          <div>
            <Button type="submit" icon={<AddRegular />} appearance="primary" disabled={isAdding || !newPolicyName.trim() || !newPolicyDocument.trim()}>
              {isAdding ? 'Adding...' : 'Add Policy'}
            </Button>
          </div>
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
              <DataGridBody<PolicyType>>
                {({ item, rowId }) => (
                  <DataGridRow<PolicyType>
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

      {/* View Policy Document Dialog */}
      <Dialog open={viewDialogOpen} onOpenChange={(_, data) => setViewDialogOpen(data.open)}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Policy: {policyToView?.name}</DialogTitle>
            <DialogContent>
              <div className={classes.documentPreview}>
                {policyToView?.document}
              </div>
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="primary">Close</Button>
              </DialogTrigger>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>

      {/* Edit Policy Document Dialog */}
      <Dialog open={editDialogOpen} onOpenChange={(_, data) => {
        if (!data.open) {
          setEditDialogOpen(false);
          setPolicyToEdit(null);
          setEditDocument('');
          setEditError('');
        }
      }}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Edit Policy: {policyToEdit?.name}</DialogTitle>
            <DialogContent>
              <Field label="Policy Document" validationMessage={editError} validationState={editError ? "error" : "none"}>
                <Textarea
                  value={editDocument}
                  onChange={(e) => setEditDocument(e.target.value)}
                  disabled={isEditing}
                  className={classes.textArea}
                  resize="vertical"
                  style={{ width: '100%', minWidth: '400px' }}
                />
              </Field>
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary" disabled={isEditing}>Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={handleEditPolicy}
                disabled={isEditing || !editDocument.trim()}
              >
                {isEditing ? 'Saving...' : 'Save'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>

      {/* Delete Confirmation Dialog */}
      <Dialog open={deleteDialogOpen} onOpenChange={(_, data) => setDeleteDialogOpen(data.open)}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Delete Policy</DialogTitle>
            <DialogContent>
              Are you sure you want to delete the policy "{policyToDelete?.name}"?
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={() => policyToDelete && handleDeletePolicy(policyToDelete)}
                disabled={isDeleting !== null}
              >
                {isDeleting ? 'Deleting...' : 'Delete'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>
    </Suspense>
    </>
  );
};

export default Policies;
